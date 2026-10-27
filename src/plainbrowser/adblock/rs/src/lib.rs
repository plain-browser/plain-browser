//! Thin C++ bridge over adblock-rust. Owned by the browser-process AdBlockService.
use adblock::lists::{FilterSet, ParseOptions};
use adblock::request::Request;
use adblock::Engine;

mod resources;

#[cxx::bridge(namespace = "plainbrowser::adblock")]
mod ffi {
    struct BlockResult {
        matched: bool,
        important: bool,
        exception: bool,
        redirect: String,
        /// URL with tracking parameters removed (`$removeparam` rules); empty when unchanged.
        rewritten_url: String,
    }

    /// Element hiding for one document.
    struct CosmeticResult {
        /// Site-specific and complex generic hide rules as one stylesheet; empty when none apply.
        css: String,
        /// A `$generichide` exception covers the document: do not ask for generic class/id rules.
        generic_hide: bool,
        /// Action filters (`:remove()`, `:style()`, `:remove-attr()`, `:remove-class()`) as adblock-rust's
        /// JSON, one filter per string; applied by the renderer's isolated-world script.
        procedural_actions: Vec<String>,
        /// Hide selectors with procedural operators (`:has-text()`, `:upward()`, `:xpath()`, ...), which are
        /// not CSS: also applied by that script. adblock-rust is built without its `css-validation` feature, so
        /// it hands them over as plain selectors; `css` never contains them.
        procedural_selectors: Vec<String>,
    }

    /// The text of one of the default lists, by name (see `DEFAULT_LISTS`), replacing the compiled-in copy.
    struct ListText {
        name: String,
        text: String,
    }

    extern "Rust" {
        type AdblockEngine;
        fn new_engine(rules: &str) -> Box<AdblockEngine>;
        /// Engine built from the filter lists compiled into the binary.
        fn new_default_engine() -> Box<AdblockEngine>;
        /// Engine built from the default lists, with each list named in `lists` taken from there instead of
        /// the compiled-in copy (downloaded updates). Unknown names are ignored.
        fn new_engine_with_lists(lists: &Vec<ListText>) -> Box<AdblockEngine>;
        /// Names of the default lists, in build order.
        fn default_list_names() -> Vec<String>;
        /// Replaces the rules with a blob made by `serialize`. Returns false (leaving the engine
        /// unchanged) when the blob is invalid. No `Result` here: Chromium builds without C++
        /// exceptions and cxx turns `Result` into `throw`.
        fn deserialize(self: &mut AdblockEngine, data: &[u8]) -> bool;
        fn serialize(self: &mut AdblockEngine) -> Vec<u8>;
        fn check_network_request(
            self: &AdblockEngine,
            url: &str,
            source_url: &str,
            request_type: &str,
        ) -> BlockResult;
        fn cosmetic_resources(self: &AdblockEngine, url: &str) -> CosmeticResult;
        /// The scriptlets (`##+js(...)`) for a document as one script for its main world, made from our
        /// resources (resources.rs); empty when none apply.
        fn scriptlets(self: &AdblockEngine, url: &str) -> String;
        /// A `data:` URL with the replacement resource when a `$redirect` rule applies to the request; empty
        /// otherwise (the request is then blocked or allowed as usual). The request type is guessed from the
        /// URL's file extension (no extension: script, then image, then xmlhttprequest), because the renderer
        /// hook that can still replace a URL does not know the request's destination.
        fn redirect_url(self: &AdblockEngine, url: &str, source_url: &str) -> String;
        /// Hosts that `$redirect` rules name (see resources::redirect_hosts), sorted.
        fn redirect_hosts(self: &AdblockEngine) -> Vec<String>;
        /// Generic hide rules for the classes and ids found in a document (adblock-rust's simple and
        /// complex class/id rules), minus the document's exceptions; empty under `$generichide`.
        fn generic_cosmetic_css(
            self: &AdblockEngine,
            url: &str,
            classes: &Vec<String>,
            ids: &Vec<String>,
        ) -> String;
    }
}

pub struct AdblockEngine {
    engine: Engine,
    redirect_hosts: Vec<String>,
}

impl AdblockEngine {
    /// An engine over `lists` with our redirect resources and scriptlets.
    fn build<'a>(lists: impl IntoIterator<Item = &'a str> + Clone) -> Box<AdblockEngine> {
        let mut set = FilterSet::new(false);
        for text in lists.clone() {
            set.add_filter_list(text.to_string(), ParseOptions::default());
        }
        let mut engine = Engine::new_with_filter_set(set);
        engine.use_resources(resources::all());
        Box::new(AdblockEngine { engine, redirect_hosts: resources::redirect_hosts(lists) })
    }
}

/// Default lists, compiled in (see ../lists/NOTICE.md). GN lists them as inputs. The names are the ones the
/// list updater uses (browser/filter_list_manifest.h).
const DEFAULT_LISTS: [(&str, &str); 4] = [
    ("easylist", include_str!("../../lists/easylist.txt")),
    ("easyprivacy", include_str!("../../lists/easyprivacy.txt")),
    ("easylistpolish", include_str!("../../lists/easylistpolish.txt")),
    ("plainbrowser-removeparam", include_str!("../../lists/plainbrowser-removeparam.txt")),
];

fn new_default_engine() -> Box<AdblockEngine> {
    new_engine_with_lists(&Vec::new())
}

fn new_engine_with_lists(lists: &Vec<ffi::ListText>) -> Box<AdblockEngine> {
    let texts: Vec<&str> = DEFAULT_LISTS
        .iter()
        .map(|(name, builtin)| lists.iter().find(|l| l.name == *name).map_or(*builtin, |l| l.text.as_str()))
        .collect();
    AdblockEngine::build(texts.iter().copied())
}

fn default_list_names() -> Vec<String> {
    DEFAULT_LISTS.iter().map(|(name, _)| name.to_string()).collect()
}

fn new_engine(rules: &str) -> Box<AdblockEngine> {
    AdblockEngine::build([rules])
}

/// adblock-rust request types to try for a URL, from its file extension (see `redirect_url`).
fn guess_request_types(url: &str) -> &'static [&'static str] {
    let path = url.split(['?', '#']).next().unwrap_or(url);
    let last = path.rsplit('/').next().unwrap_or("");
    let ext = last.rsplit_once('.').map(|(_, e)| e.to_ascii_lowercase()).unwrap_or_default();
    match ext.as_str() {
        "js" | "mjs" => &["script"],
        "gif" | "png" | "jpg" | "jpeg" | "webp" | "svg" | "ico" | "avif" => &["image"],
        "css" => &["stylesheet"],
        "json" => &["xmlhttprequest"],
        "html" | "htm" => &["sub_frame"],
        "mp4" | "webm" | "mp3" | "m4a" | "ogg" => &["media"],
        _ => &["script", "image", "xmlhttprequest"],
    }
}

impl AdblockEngine {
    fn deserialize(&mut self, data: &[u8]) -> bool {
        let mut e = Engine::default();
        if e.deserialize(data).is_ok() {
            e.use_resources(resources::all());
            self.engine = e;
            // The blob does not carry the list texts the redirect hosts come from.
            self.redirect_hosts.clear();
            true
        } else {
            false
        }
    }

    fn serialize(&mut self) -> Vec<u8> {
        self.engine.serialize()
    }

    fn check_network_request(&self, url: &str, source_url: &str, request_type: &str) -> ffi::BlockResult {
        match Request::new(url, source_url, request_type, "GET") {
            Ok(req) => {
                let r = self.engine.check_network_request(&req);
                ffi::BlockResult {
                    matched: r.should_block(),
                    important: r.important,
                    exception: r.exception.is_some(),
                    redirect: r.redirect.unwrap_or_default(),
                    rewritten_url: r.rewritten_url.unwrap_or_default(),
                }
            }
            Err(_) => ffi::BlockResult {
                matched: false,
                important: false,
                exception: false,
                redirect: String::new(),
                rewritten_url: String::new(),
            },
        }
    }

    /// Site-specific hide rules (and complex generic ones) as one stylesheet, plus the `$generichide` flag.
    fn cosmetic_resources(&self, url: &str) -> ffi::CosmeticResult {
        let res = self.engine.url_cosmetic_resources(url);
        let mut procedural_actions: Vec<String> = res.procedural_actions.into_iter().collect();
        procedural_actions.sort();
        let (css, mut procedural_selectors): (Vec<String>, Vec<String>) =
            res.hide_selectors.into_iter().map(normalize_selector).partition(|s| !is_procedural(s));
        procedural_selectors.sort();
        ffi::CosmeticResult {
            css: hide_css(css),
            generic_hide: res.generichide,
            procedural_actions,
            procedural_selectors,
        }
    }

    fn scriptlets(&self, url: &str) -> String {
        self.engine.url_cosmetic_resources(url).injected_script
    }

    fn redirect_url(&self, url: &str, source_url: &str) -> String {
        for request_type in guess_request_types(url) {
            if let Ok(req) = Request::new(url, source_url, request_type, "GET") {
                let r = self.engine.check_network_request(&req);
                if let Some(redirect) = r.redirect {
                    if r.exception.is_none() && redirect.starts_with("data:") {
                        return redirect;
                    }
                }
            }
        }
        String::new()
    }

    fn redirect_hosts(&self) -> Vec<String> {
        self.redirect_hosts.clone()
    }

    fn generic_cosmetic_css(&self, url: &str, classes: &Vec<String>, ids: &Vec<String>) -> String {
        if classes.is_empty() && ids.is_empty() {
            return String::new();
        }
        let res = self.engine.url_cosmetic_resources(url);
        if res.generichide {
            return String::new();
        }
        // Generic procedural rules are not a thing lists write; drop any rather than break the stylesheet.
        hide_css(
            self.engine
                .hidden_class_id_selectors(classes.iter(), ids.iter(), &res.exceptions)
                .into_iter()
                .map(normalize_selector)
                .filter(|s| !is_procedural(s)),
        )
    }
}

/// `display: none` for the selectors, one rule per selector (Blink drops a whole rule when any selector in its
/// list is invalid, and lists contain selectors Blink does not know), sorted so the stylesheet is stable; empty
/// for no selectors.
fn hide_css(selectors: impl IntoIterator<Item = String>) -> String {
    let mut sels: Vec<String> = selectors.into_iter().collect();
    sels.sort();
    sels.dedup();
    let mut css = String::new();
    for sel in sels {
        css.push_str(&sel);
        css.push_str(" { display: none !important; }\n");
    }
    css
}

/// Operators that are not CSS (uBlock Origin and Adblock Plus extended syntax). A selector with one of them
/// goes to the renderer's procedural script instead of the stylesheet.
const PROCEDURAL_OPERATORS: [&str; 19] = [
    ":has-text(", ":-abp-contains(", ":contains(", ":upward(", ":xpath(", ":matches-css(",
    ":matches-css-before(", ":matches-css-after(", ":min-text-length(", ":matches-attr(", ":matches-path(",
    ":matches-prop(", ":watch-attr(", ":others(", ":if(", ":if-not(", ":-abp-properties(", ":matches-media(",
    ":remove(",
];

fn is_procedural(selector: &str) -> bool {
    PROCEDURAL_OPERATORS.iter().any(|op| selector.contains(op))
}

/// Adblock Plus spellings of operators Blink or our script know under another name.
fn normalize_selector(selector: String) -> String {
    if selector.contains(":-abp-") {
        selector.replace(":-abp-has(", ":has(").replace(":-abp-contains(", ":has-text(")
    } else {
        selector
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const RULES: &str = "||ads.example.com^\n@@||ads.example.com/ok.js\nexample.org##.banner\n||track.example.net^$important\n";

    #[test]
    fn blocks_and_allows() {
        let e = new_engine(RULES);
        assert!(e.check_network_request("https://ads.example.com/x.js", "https://site.com", "script").matched);
        let ok = e.check_network_request("https://ads.example.com/ok.js", "https://site.com", "script");
        assert!(!ok.matched && ok.exception);
        assert!(!e.check_network_request("https://site.com/app.js", "https://site.com", "script").matched);
        assert!(e.check_network_request("https://track.example.net/p", "https://site.com", "image").important);
    }

    #[test]
    fn cosmetic() {
        let e = new_engine(RULES);
        assert!(e.cosmetic_resources("https://example.org/").css.contains(".banner"));
        assert!(e.cosmetic_resources("https://other.org/").css.is_empty());
        assert!(!e.cosmetic_resources("https://example.org/").generic_hide);
    }

    const GENERIC: &str = "##.ad-box\n###top-ad\n##.promo > .sponsored\nexample.org#@#.ad-box\n@@||quiet.example^$generichide\n";

    fn strings(v: &[&str]) -> Vec<String> {
        v.iter().map(|s| s.to_string()).collect()
    }

    #[test]
    fn generic_class_and_id_rules() {
        let e = new_engine(GENERIC);
        // Simple generic rules are not in the per-site stylesheet: they need the page's classes and ids.
        assert!(!e.cosmetic_resources("https://site.com/").css.contains(".ad-box"));
        let css = e.generic_cosmetic_css("https://site.com/", &strings(&["ad-box", "content", "promo"]), &strings(&["top-ad"]));
        assert!(css.contains(".ad-box"), "{css}");
        assert!(css.contains("#top-ad"), "{css}");
        assert!(css.contains(".promo > .sponsored"), "{css}");
        assert!(!css.contains("content"), "{css}");
        // Nothing found on the page, or no matching class: no stylesheet.
        assert!(e.generic_cosmetic_css("https://site.com/", &strings(&[]), &strings(&[])).is_empty());
        assert!(e.generic_cosmetic_css("https://site.com/", &strings(&["article"]), &strings(&[])).is_empty());
    }

    #[test]
    fn generic_rules_honour_exceptions_and_generichide() {
        let e = new_engine(GENERIC);
        // example.org#@#.ad-box: that one rule is off on example.org, the id rule still applies.
        let css = e.generic_cosmetic_css("https://example.org/", &strings(&["ad-box"]), &strings(&["top-ad"]));
        assert!(!css.contains(".ad-box"), "{css}");
        assert!(css.contains("#top-ad"), "{css}");
        // $generichide: no generic rules at all, and the flag tells the renderer not to ask.
        assert!(e.cosmetic_resources("https://quiet.example/").generic_hide);
        assert!(e.generic_cosmetic_css("https://quiet.example/", &strings(&["ad-box"]), &strings(&["top-ad"])).is_empty());
    }

    #[test]
    fn downloaded_lists_replace_the_builtin_ones() {
        assert_eq!(default_list_names(), strings(&["easylist", "easyprivacy", "easylistpolish", "plainbrowser-removeparam"]));
        let lists = vec![ffi::ListText { name: "easylist".into(), text: "||fresh-ads.example^\n".into() }];
        let e = new_engine_with_lists(&lists);
        assert!(e.check_network_request("https://fresh-ads.example/a.js", "https://site.com", "script").matched);
        // EasyList was replaced: its well-known host is no longer blocked by it...
        assert!(!e.check_network_request("https://pagead2.googlesyndication.com/pagead/js/adsbygoogle.js", "https://site.com", "script").matched);
        // ...while the other lists are still the compiled-in ones.
        let r = e.check_network_request("https://example.com/p?utm_source=x&q=1", "https://example.com/", "main_frame");
        assert_eq!(r.rewritten_url, "https://example.com/p?q=1");
        // Unknown names are ignored.
        let e = new_engine_with_lists(&vec![ffi::ListText { name: "other".into(), text: "||x.example^".into() }]);
        assert!(!e.check_network_request("https://x.example/", "https://site.com", "script").matched);
    }

    #[test]
    fn default_lists() {
        let e = new_default_engine();
        // A well-known ad server from EasyList; must stay blocked.
        assert!(e.check_network_request("https://pagead2.googlesyndication.com/pagead/js/adsbygoogle.js", "https://example.com", "script").matched);
        assert!(!e.check_network_request("https://example.com/app.js", "https://example.com", "script").matched);
    }

    #[test]
    fn strips_tracking_params() {
        let e = new_engine("*$removeparam=utm_source\n*$removeparam=fbclid\n");
        let r = e.check_network_request(
            "https://site.com/a?utm_source=nl&id=1&fbclid=abc",
            "https://other.com/",
            "main_frame",
        );
        assert!(!r.matched);
        assert_eq!(r.rewritten_url, "https://site.com/a?id=1");
        // Nothing to strip: no rewrite.
        assert!(e.check_network_request("https://site.com/a?id=1", "https://other.com/", "main_frame").rewritten_url.is_empty());
        assert!(e.check_network_request("https://site.com/a", "https://other.com/", "main_frame").rewritten_url.is_empty());
    }

    #[test]
    fn default_removeparam_list() {
        let e = new_default_engine();
        let r = e.check_network_request(
            "https://example.com/p?utm_campaign=x&utm_medium=y&gclid=1&q=2",
            "https://example.com/",
            "main_frame",
        );
        assert_eq!(r.rewritten_url, "https://example.com/p?q=2");
        // Subframes and plain subresources are covered by the same rules.
        let r = e.check_network_request("https://example.com/f?fbclid=1", "https://example.com/", "sub_frame");
        assert_eq!(r.rewritten_url, "https://example.com/f");
    }

    const EXTRA: &str = "||ads.example^$redirect=noopjs,script\n\
                         ||pixel.example^$image,redirect=1x1.gif\n\
                         @@||ads.example/ok.js$script\n\
                         example.org##+js(set, adsEnabled, false)\n\
                         example.org##+js(nostif, adblock, 1000)\n\
                         example.org##+js(unknown-scriptlet, x)\n\
                         example.org#?#.card:has-text(Sponsored)\n\
                         example.org##.box:style(margin: 0 !important)\n\
                         example.org##.popup:remove()\n";

    #[test]
    fn redirect_resources() {
        let e = new_engine(EXTRA);
        let r = e.redirect_url("https://ads.example/tag.js", "https://site.com/");
        assert!(r.starts_with("data:application/javascript;base64,"), "{r}");
        // No extension: tried as a script first.
        assert!(e.redirect_url("https://ads.example/serve", "https://site.com/").starts_with("data:application/javascript"));
        assert!(e.redirect_url("https://pixel.example/p.gif", "https://site.com/").starts_with("data:image/gif;base64,R0lGOD"));
        // The rule is for scripts: an image from the same host is not redirected.
        assert_eq!(e.redirect_url("https://ads.example/banner.png", "https://site.com/"), "");
        // Exception and unrelated hosts.
        assert_eq!(e.redirect_url("https://ads.example/ok.js", "https://site.com/"), "");
        assert_eq!(e.redirect_url("https://other.example/x.js", "https://site.com/"), "");
        assert_eq!(e.redirect_hosts(), vec!["ads.example".to_string(), "pixel.example".to_string()]);
        // The bundled lists name hosts too (EasyPrivacy's redirect rules).
        assert!(!new_default_engine().redirect_hosts().is_empty());
    }

    #[test]
    fn scriptlets_use_our_resources() {
        let e = new_engine(EXTRA);
        let js = e.scriptlets("https://example.org/");
        assert!(js.contains("function pbSetConstant("), "{js}");
        assert!(js.contains("pbSetConstant(\"adsEnabled\", \"false\")") || js.contains("pbSetConstant('adsEnabled', 'false')"), "{js}");
        assert!(js.contains("function pbDefuseTimer("), "{js}");
        assert!(js.contains("function pbPattern("), "{js}");
        // Each helper once, and nothing for a scriptlet we do not have.
        assert_eq!(js.matches("function pbPattern(").count(), 1);
        assert!(!js.contains("unknown-scriptlet"));
        assert!(e.scriptlets("https://other.org/").is_empty());
    }

    #[test]
    fn procedural_actions_as_json() {
        let e = new_engine(EXTRA);
        let res = e.cosmetic_resources("https://example.org/");
        let all = res.procedural_actions.join("\n");
        assert!(all.contains(r#""type":"style""#), "{all}");
        assert!(all.contains(r#""type":"remove""#), "{all}");
        assert!(e.cosmetic_resources("https://other.org/").procedural_actions.is_empty());
        // :has-text() is not CSS: it goes to the script, not into the stylesheet.
        assert_eq!(res.procedural_selectors, vec![".card:has-text(Sponsored)".to_string()]);
        assert!(!res.css.contains("has-text"), "{}", res.css);
        // EasyList's #?# rules come through on their sites, and :-abp-has() becomes CSS :has().
        let d = new_default_engine();
        let r = d.cosmetic_resources("https://www.cjleo.com/");
        assert!(r.procedural_selectors.iter().any(|a| a.contains(":has-text(")), "{:?}", r.procedural_selectors);
        let n = new_engine("example.net#?#div:-abp-has(> .ad)\nexample.net#?#p:-abp-contains(Ad)\n");
        let r = n.cosmetic_resources("https://example.net/");
        assert!(r.css.contains("div:has(> .ad) { display: none !important; }"), "{}", r.css);
        assert_eq!(r.procedural_selectors, vec!["p:has-text(Ad)".to_string()]);
    }

    #[test]
    fn roundtrip() {
        let mut e = new_engine(RULES);
        let bytes = e.serialize();
        let mut e2 = new_engine("");
        assert!(e2.deserialize(&bytes));
        assert!(e2.check_network_request("https://ads.example.com/x.js", "https://site.com", "script").matched);
        // An invalid blob is rejected and leaves the engine usable.
        assert!(!e2.deserialize(b"not a blob"));
        assert!(e2.check_network_request("https://ads.example.com/x.js", "https://site.com", "script").matched);
    }
}
