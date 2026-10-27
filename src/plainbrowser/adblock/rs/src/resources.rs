//! Our own `$redirect` resources and `+js(...)` scriptlets (../../resources, MPL-2.0), under the names and
//! aliases the filter lists use. Compiled in; GN lists the files as inputs.
use adblock::resources::{MimeType, PermissionMask, Resource, ResourceType};

/// Standard base64 (adblock-rust stores resource content base64-encoded).
fn base64(data: &[u8]) -> String {
    const ALPHABET: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    let mut out = String::with_capacity((data.len() + 2) / 3 * 4);
    for chunk in data.chunks(3) {
        let b = [chunk[0], *chunk.get(1).unwrap_or(&0), *chunk.get(2).unwrap_or(&0)];
        let n = (u32::from(b[0]) << 16) | (u32::from(b[1]) << 8) | u32::from(b[2]);
        for i in 0..4 {
            if i <= chunk.len() {
                out.push(ALPHABET[(n >> (18 - 6 * i) & 63) as usize] as char);
            } else {
                out.push('=');
            }
        }
    }
    out
}

fn resource(name: &str, aliases: &[&str], kind: ResourceType, content: &[u8], deps: &[&str]) -> Resource {
    Resource {
        name: name.to_string(),
        aliases: aliases.iter().map(|a| a.to_string()).collect(),
        kind,
        content: base64(content),
        dependencies: deps.iter().map(|d| d.to_string()).collect(),
        permission: PermissionMask::default(),
    }
}

macro_rules! redirect {
    ($name:literal, [$($alias:literal),*], $mime:expr) => {
        resource($name, &[$($alias),*], ResourceType::Mime($mime),
                 include_bytes!(concat!("../../resources/redirects/", $name)), &[])
    };
}

macro_rules! scriptlet {
    ($name:literal, [$($alias:literal),*], [$($dep:literal),*]) => {
        resource($name, &[$($alias),*], ResourceType::Mime(MimeType::ApplicationJavascript),
                 include_bytes!(concat!("../../resources/scriptlets/", $name)), &[$($dep),*])
    };
}

macro_rules! helper {
    ($name:literal) => {
        resource($name, &[], ResourceType::Mime(MimeType::FnJavascript),
                 include_bytes!(concat!("../../resources/scriptlets/", $name, ".js")), &[])
    };
}

/// Every resource, for `Engine::use_resources`.
pub fn all() -> Vec<Resource> {
    use MimeType::*;
    vec![
        redirect!("noop.js", ["noopjs", "noop.js"], ApplicationJavascript),
        redirect!("noop.txt", ["nooptext", "noop.txt"], TextPlain),
        redirect!("noop.json", ["noopjson", "noop.json"], ApplicationJson),
        redirect!("noop.html", ["noopframe", "noop.html"], TextHtml),
        redirect!("noop.css", ["noopcss"], TextCss),
        redirect!("1x1.gif", ["1x1-transparent.gif"], ImageGif),
        redirect!("2x2.png", ["2x2-transparent.png"], ImagePng),
        redirect!("googletagmanager_gtm.js", ["googletagmanager.com/gtm.js"], ApplicationJavascript),
        redirect!("google-analytics_analytics.js", ["google-analytics.com/analytics.js", "googletagmanager_gtag.js", "googletagmanager.com/gtag/js"], ApplicationJavascript),
        redirect!("googlesyndication_adsbygoogle.js", ["googlesyndication.com/adsbygoogle.js"], ApplicationJavascript),
        redirect!("chartbeat.js", ["static.chartbeat.com/chartbeat.js"], ApplicationJavascript),
        redirect!("fingerprint2.js", [], ApplicationJavascript),
        helper!("pb-utils.fn"),
        helper!("pb-timers.fn"),
        helper!("pb-storage.fn"),
        scriptlet!("set-constant.js", ["set.js"], ["pb-utils.fn"]),
        scriptlet!("abort-on-property-read.js", ["aopr.js"], ["pb-utils.fn"]),
        scriptlet!("abort-on-property-write.js", ["aopw.js"], ["pb-utils.fn"]),
        scriptlet!("abort-current-script.js", ["acs.js", "abort-current-inline-script.js", "acis.js"], ["pb-utils.fn"]),
        scriptlet!("abort-on-stack-trace.js", ["aost.js"], ["pb-utils.fn"]),
        scriptlet!("remove-node-text.js", ["rmnt.js"], ["pb-utils.fn"]),
        scriptlet!("no-fetch-if.js", ["prevent-fetch.js"], ["pb-utils.fn"]),
        scriptlet!("no-xhr-if.js", ["prevent-xhr.js"], ["pb-utils.fn"]),
        scriptlet!("addEventListener-defuser.js", ["aeld.js", "prevent-addEventListener.js"], ["pb-utils.fn"]),
        scriptlet!("no-setTimeout-if.js", ["nostif.js", "prevent-setTimeout.js", "setTimeout-defuser.js"], ["pb-utils.fn", "pb-timers.fn"]),
        scriptlet!("no-setInterval-if.js", ["nosiif.js", "prevent-setInterval.js", "setInterval-defuser.js"], ["pb-utils.fn", "pb-timers.fn"]),
        scriptlet!("set-local-storage-item.js", [], ["pb-utils.fn", "pb-storage.fn"]),
        scriptlet!("set-session-storage-item.js", [], ["pb-utils.fn", "pb-storage.fn"]),
        scriptlet!("set-cookie.js", [], ["pb-utils.fn"]),
        scriptlet!("cookie-remover.js", ["remove-cookie.js"], ["pb-utils.fn"]),
        scriptlet!("json-prune.js", [], ["pb-utils.fn"]),
        scriptlet!("remove-attr.js", ["ra.js"], ["pb-utils.fn"]),
        scriptlet!("remove-class.js", ["rc.js"], ["pb-utils.fn"]),
        scriptlet!("window.open-defuser.js", ["nowoif.js", "no-window-open-if.js", "prevent-window-open.js"], ["pb-utils.fn"]),
    ]
}

/// The hosts named by `$redirect`/`$redirect-rule` network rules (`||host^...$...,redirect=...`) in `lists`. The
/// renderer asks the browser for a redirect only for requests to these hosts and their subdomains (all others
/// would get the plain block anyway); rules without a `||host` anchor are left to blocking.
pub fn redirect_hosts<'a>(lists: impl IntoIterator<Item = &'a str>) -> Vec<String> {
    let mut hosts = std::collections::BTreeSet::new();
    for list in lists {
        for line in list.lines() {
            let line = line.trim();
            if !line.starts_with("||") {
                continue;
            }
            let Some(options) = line.rfind('$').map(|i| &line[i + 1..]) else { continue };
            if !options.split(',').any(|o| o.starts_with("redirect=") || o.starts_with("redirect-rule=")) {
                continue;
            }
            let host: String = line[2..]
                .chars()
                .take_while(|c| c.is_ascii_alphanumeric() || *c == '.' || *c == '-')
                .collect::<String>()
                .to_ascii_lowercase();
            if host.contains('.') && !host.starts_with('.') && !host.ends_with('.') {
                hosts.insert(host);
            }
        }
    }
    hosts.into_iter().collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn base64_matches_the_standard() {
        assert_eq!(base64(b""), "");
        assert_eq!(base64(b"f"), "Zg==");
        assert_eq!(base64(b"fo"), "Zm8=");
        assert_eq!(base64(b"foo"), "Zm9v");
        assert_eq!(base64(b"foobar"), "Zm9vYmFy");
    }

    #[test]
    fn scriptlets_start_with_a_named_function() {
        // adblock-rust calls a scriptlet by the name of the function its text starts with.
        let dir = format!("{}/../resources/scriptlets", env!("CARGO_MANIFEST_DIR"));
        for entry in std::fs::read_dir(dir).unwrap() {
            let path = entry.unwrap().path();
            let name = path.file_name().unwrap().to_string_lossy().to_string();
            if name.ends_with(".fn.js") {
                continue;
            }
            let text = std::fs::read_to_string(&path).unwrap();
            assert!(text.starts_with("function pb"), "{name}");
            assert!(all().iter().any(|r| r.name == name), "{name} is not registered");
        }
    }

    #[test]
    fn redirect_hosts_from_rules() {
        let list = "||ads.example^$redirect=noopjs,script\n\
                    ||Track.Example.NET/x.js$important,redirect-rule=noop.js\n\
                    ||plain.example^$script\n\
                    /generic-ad.js$redirect=noopjs\n\
                    @@||ads.example^$redirect=noopjs\n\
                    ! ||comment.example^$redirect=noopjs\n";
        assert_eq!(redirect_hosts([list]), vec!["ads.example".to_string(), "track.example.net".to_string()]);
    }
}
