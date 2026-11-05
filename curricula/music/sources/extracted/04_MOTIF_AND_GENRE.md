# Distilled Learnings — Motif Development & Genre Form

Source: resone_song_design_genre_guide.md (selected high-signal rules)

## Global rules worth baking into a wind

- Prefer transforming remembered material over constantly replacing it.
  Useful transforms: octave transfer, register expansion, rhythmic displacement, fragmentation, inversion, reharmonization, pedal reinterpretation, density growth, call/response, AHD, motivic reassertion.
- Different sections should differ in at least two dimensions (density, register, harmony, rhythm, phrase length, articulation, bass, melodic range, repetition rate, dynamics).
- Earn the climax: combine previously established identities at greater scale rather than introduce unrelated material.
- User request remains authoritative; genre shapes realization.

## Compact genre cores (for retrieval-style training)

- **Rock**: memorable sections, riff/chord identity, physical energy changes. Avoid changing the central riff constantly.
- **Post-rock**: long arcs from a small seed. Quiet → repetition → layer → wider → rhythmic activation → climax → aftermath. Avoid unrelated motif proliferation.
- **Metal**: riff identity often > conventional progression. Avoid chord-pad behavior under every riff.
- **Progressive metal**: development, not mere complexity. Motif seed → expansion → metric reinterpretation → harmonic divergence → climax → memory return.
- **Trance / Uplifting trance**: long-form tension/release through repetition. One foreign pitch can gain meaning over repeated exposures. Climax makes the theme physically enormous.
- **Classical**: develop motives rather than merely loop them. Gesture → phrase → period → section.
- **Cinematic orchestral** (modifier): early motif establishment, later large-scale return, foreground/background hierarchy.

## Training signal

- Given a seed motif + genre id + section role, the wind must apply at least one named transform and satisfy the “differ in two dimensions” or “earn the climax” rule as appropriate.
- Held-out tasks should change surface genre or surface motif while keeping the same abstract development demand.
