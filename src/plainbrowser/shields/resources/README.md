# Shields strings

`plainbrowser_shields_strings.grdp` holds every user-visible string of `//plainbrowser/shields`: the toolbar
button, its bubble and the "Sites with protections off" section on `chrome://settings/privacy`. It is a
`<part>` of `chrome/app/generated_resources.grd` (`patches/60-ui/0008`), so C++ uses the `IDS_` ids from
`chrome/grit/generated_resources.h` and `l10n_util`, and the settings page gets them through
`ui/shields_settings_strings.cc` (`$i18n{plainBrowserShields...}`). The strings end up in the normal locale
`.pak` files; no repack or `resource_ids` change is needed.

Translations: one `plainbrowser_shields_strings_<lang>.xtb` per language, listed in the same grd's
`<translations>` next to Chromium's own `generated_resources_<lang>.xtb` (grit merges several files of one
language). Today: `pl`. A language without a file falls back to English.

## Adding or changing a string

1. Edit the `.grdp`. Keep `meaning="Plain Browser shields"` on every message: the meaning is part of grit's
   fingerprint, so our translation ids never equal a Chromium message's id. Without it, "Add" would share its id
   with Chromium's "Add", and grit asserts when two xtb files translate the same id.
2. Get the ids. They are grit's fingerprints of the English text (placeholders by name), so the text and the id
   change together:

   ```
   scripts/grit-ids.py --grit <chromium>/tools/grit src/plainbrowser/shields/resources/plainbrowser_shields_strings.grdp
   ```

   grit is pure Python; a copy of `tools/grit` from the build tree is enough.
3. Update each `.xtb` and check it:

   ```
   scripts/grit-ids.py --grit <chromium>/tools/grit src/plainbrowser/shields/resources/plainbrowser_shields_strings.grdp \
       --xtb src/plainbrowser/shields/resources/plainbrowser_shields_strings_pl.xtb
   ```

   It reports unknown ids, missing translations and placeholder mismatches (`0 problem(s)` when clean).
4. A new language: add the `.xtb`, a `<file>` line in `patches/60-ui/0008` (grd `<translations>`) and its path in
   the same patch's `.gritdeps` hunk. `generated_resources.grd.gritdeps` must list exactly the grd's inputs,
   sorted; the build checks it.

Plural messages use ICU syntax (`{COUNT, plural, =1 {...} other {# ...}}`) and
`l10n_util::GetPluralStringFUTF16`; the Polish translation needs the `few` and `many` forms.
