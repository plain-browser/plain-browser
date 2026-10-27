//! Thin C++ bridge over adblock-rust. Owned by the browser-process AdBlockService.
use adblock::lists::{FilterSet, ParseOptions};
use adblock::request::Request;
use adblock::Engine;

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

pub struct AdblockEngine(Engine);

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
    let mut set = FilterSet::new(false);
    for (name, builtin) in DEFAULT_LISTS {
        let text = lists.iter().find(|l| l.name == name).map_or(builtin, |l| l.text.as_str());
        set.add_filter_list(text.to_string(), ParseOptions::default());
    }
    Box::new(AdblockEngine(Engine::new_with_filter_set(set)))
}

fn default_list_names() -> Vec<String> {
    DEFAULT_LISTS.iter().map(|(name, _)| name.to_string()).collect()
}

fn new_engine(rules: &str) -> Box<AdblockEngine> {
    let mut set = FilterSet::new(false);
    set.add_filter_list(rules.to_string(), ParseOptions::default());
    Box::new(AdblockEngine(Engine::new_with_filter_set(set)))
}

impl AdblockEngine {
    fn deserialize(&mut self, data: &[u8]) -> bool {
        let mut e = Engine::default();
        if e.deserialize(data).is_ok() {
            self.0 = e;
            true
        } else {
            false
        }
    }

    fn serialize(&mut self) -> Vec<u8> {
        self.0.serialize()
    }

    fn check_network_request(&self, url: &str, source_url: &str, request_type: &str) -> ffi::BlockResult {
        match Request::new(url, source_url, request_type, "GET") {
            Ok(req) => {
                let r = self.0.check_network_request(&req);
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
        let res = self.0.url_cosmetic_resources(url);
        ffi::CosmeticResult {
            css: hide_css(res.hide_selectors),
            generic_hide: res.generichide,
        }
    }

    fn generic_cosmetic_css(&self, url: &str, classes: &Vec<String>, ids: &Vec<String>) -> String {
        if classes.is_empty() && ids.is_empty() {
            return String::new();
        }
        let res = self.0.url_cosmetic_resources(url);
        if res.generichide {
            return String::new();
        }
        hide_css(self.0.hidden_class_id_selectors(classes.iter(), ids.iter(), &res.exceptions))
    }
}

/// `display: none` for the selectors, sorted so the stylesheet is stable; empty for no selectors.
fn hide_css(selectors: impl IntoIterator<Item = String>) -> String {
    let mut sels: Vec<String> = selectors.into_iter().collect();
    if sels.is_empty() {
        return String::new();
    }
    sels.sort();
    sels.dedup();
    format!("{} {{ display: none !important; }}", sels.join(",\n"))
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
