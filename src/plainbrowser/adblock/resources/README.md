# Redirect resources and scriptlets

Our own implementations, MPL-2.0, of what filter lists ask for with `$redirect=<name>` and `##+js(<name>, ...)`.
`rs/src/resources.rs` registers every file here under the names and aliases the lists use and hands them to
adblock-rust (`Engine::use_resources`); GN lists the files as inputs of the Rust library.

- `redirects/`: served as `data:` URLs instead of a blocked request (`renderer/request_redirector.cc`).
- `scriptlets/`: injected into the page's main world before its own scripts (`renderer/cosmetic_filter_agent.cc`).
  Each file must start with `function pb<Name>(`: adblock-rust calls that function with the rule's arguments.
  `*.fn.js` are shared helpers (dependencies in `resources.rs`); adblock-rust puts each needed helper once in front
  of the calls, and the agent wraps the whole script in a function so none of it becomes a page global.

We do not copy uBlock Origin's or AdGuard's resources (GPLv3). A rule naming a scriptlet we do not have is skipped.

Adding one: write the file, register it in `resources.rs` (name, aliases, helpers), add it to the `inputs` of
`//plainbrowser/adblock:ffi` in `BUILD.gn`, and a test in `scriptlets_test.js`
(`node --test src/plainbrowser/adblock/resources/scriptlets_test.js`). `cargo test` checks that every scriptlet
file starts with its function and is registered.
