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

    extern "Rust" {
        type AdblockEngine;
        fn new_engine(rules: &str) -> Box<AdblockEngine>;
        /// Engine built from the filter lists compiled into the binary.
        fn new_default_engine() -> Box<AdblockEngine>;
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
        fn cosmetic_css(self: &AdblockEngine, url: &str) -> String;
    }
}

pub struct AdblockEngine(Engine);

/// Default lists, compiled in (see ../lists/NOTICE.md). GN lists them as inputs.
const DEFAULT_LISTS: [&str; 4] = [
    include_str!("../../lists/easylist.txt"),
    include_str!("../../lists/easyprivacy.txt"),
    include_str!("../../lists/easylistpolish.txt"),
    include_str!("../../lists/plainbrowser-removeparam.txt"),
];

fn new_default_engine() -> Box<AdblockEngine> {
    let mut set = FilterSet::new(false);
    for list in DEFAULT_LISTS {
        set.add_filter_list(list.to_string(), ParseOptions::default());
    }
    Box::new(AdblockEngine(Engine::new_with_filter_set(set)))
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

    /// Generic + site-specific hide rules as one stylesheet.
    fn cosmetic_css(&self, url: &str) -> String {
        let res = self.0.url_cosmetic_resources(url);
        if res.hide_selectors.is_empty() {
            return String::new();
        }
        let mut sels: Vec<_> = res.hide_selectors.into_iter().collect();
        sels.sort();
        format!("{} {{ display: none !important; }}", sels.join(",\n"))
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
        assert!(e.cosmetic_css("https://example.org/").contains(".banner"));
        assert!(e.cosmetic_css("https://other.org/").is_empty());
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
