//! Thin C++ bridge over adblock-rust. Owned by the browser-process AdBlockService.
use adblock::lists::{FilterSet, ParseOptions};
use adblock::request::Request;
use adblock::Engine;

#[cxx::bridge(namespace = "draftway::adblock")]
mod ffi {
    struct BlockResult {
        matched: bool,
        important: bool,
        exception: bool,
        redirect: String,
    }

    extern "Rust" {
        type AdblockEngine;
        fn new_engine(rules: &str) -> Box<AdblockEngine>;
        /// Engine built from the filter lists compiled into the binary.
        fn new_default_engine() -> Box<AdblockEngine>;
        fn deserialize_engine(data: &[u8]) -> Result<Box<AdblockEngine>>;
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
const DEFAULT_LISTS: [&str; 3] = [
    include_str!("../../lists/easylist.txt"),
    include_str!("../../lists/easyprivacy.txt"),
    include_str!("../../lists/easylistpolish.txt"),
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

fn deserialize_engine(data: &[u8]) -> Result<Box<AdblockEngine>, String> {
    let mut e = Engine::default();
    e.deserialize(data).map_err(|err| format!("{err:?}"))?;
    Ok(Box::new(AdblockEngine(e)))
}

impl AdblockEngine {
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
                }
            }
            Err(_) => ffi::BlockResult { matched: false, important: false, exception: false, redirect: String::new() },
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
    fn roundtrip() {
        let mut e = new_engine(RULES);
        let bytes = e.serialize();
        let e2 = deserialize_engine(&bytes).unwrap();
        assert!(e2.check_network_request("https://ads.example.com/x.js", "https://site.com", "script").matched);
    }
}
