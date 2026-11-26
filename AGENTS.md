# Win-Butler Repository Instructions

## Start Here

Before changing code or project documentation, inspect `git status`, read this file, and read `docs/DEVELOPMENT_PLAN.md`. For a specific module, also read its technical design, validation record, and reference analysis when present. This is the sole repository rule file; do not create an alternate rule file.

Treat the current source tree, build files, and existing user changes as authoritative. Preserve unrelated changes and keep implementation within the ownership boundaries below.

## Project Scope

Win-Butler is a native Windows desktop utility. Prioritize correct Windows behavior, low idle resource use, reversible system changes, and standard Chinese computer terminology.

## Repository Boundaries

- Read the repository's existing project and build files before making changes. Follow their current directory structure and naming conventions.
- Do not create new top-level directories, test directories, sample trees, generated output directories, or architectural layers unless the current task proves they are required and the reason is recorded in the implementation summary.
- Put a new source file beside the existing module that owns its behavior. Put resources with the existing resource files. Put project documentation in `docs/`.
- Do not add a second framework, package manager, UI toolkit, or service layer when the repository already has an established equivalent.
- Do not reorganize unrelated files or perform broad formatting/refactoring while implementing a feature.

## Directory Map

- `src/app/`: EUI-NEO desktop entry point, views, view state, and user interaction.
- `src/shell/`: Windows Shell, registry, package-manifest discovery, and context-menu catalog APIs.
- `src/tools/`: small command-line diagnostics that reuse the native libraries.
- `tests/unit/`: existing focused unit tests; optional for local verification and not a reason to add test infrastructure.
- `fixtures/`: existing reversible local Shell/ShellEx/Packaged COM samples; do not add VM, integration, or end-to-end fixture trees.
- `docs/`: the development plan, technical designs, reference analyses, and local validation records.
- `build/`: generated local CMake output; do not commit generated files.

Keep new code in these existing ownership areas by default. A new `src/` subdirectory is allowed only when a phase needs a distinct reusable module and the plan records its responsibility; do not add new top-level directories or create one directory per tiny feature.

## Documentation Layout and Naming

Keep project development documents under `docs/`. Root-level exceptions are `AGENTS.md`, `README.md`, `README.en.md`, `LICENSE`, `THIRD_PARTY_NOTICES.md`, and other files required by repository hosting or package conventions.

- `docs/DEVELOPMENT_PLAN.md`: the single canonical product scope, phase sequence, acceptance criteria, and per-phase AI prompts. Do not keep a duplicate plan in the repository root.
- `<SUBJECT>_TECHNICAL_DESIGN.md`: module architecture, data models, APIs, and operational details, for example `CONTEXT_MENU_TECHNICAL_DESIGN.md`.
- `<MILESTONE>_TECHNICAL_VALIDATION.md`: dated evidence from local build/run and manual checks, for example `M0_TECHNICAL_VALIDATION.md`.
- `REFERENCE_ANALYSIS[_<SUBJECT>].md`: reference feature comparison, source notes, and license boundaries.
- `PLATFORM_COMPATIBILITY.md`: OS/build/API compatibility facts, only when such a separate document is needed.

Use uppercase ASCII names with words separated by underscores. Keep one document per purpose, update links when moving or renaming a document, and remove obsolete documents instead of keeping competing plans. Compatibility notes do not imply a VM matrix or cross-version test requirement.

## Windows and C++ Engineering

- Keep the application native C++; do not introduce Electron or WebView.
- Prefer Win32, COM, Shell, Service Control Manager, Windows Firewall COM APIs, registry APIs, PDH, and other supported Windows APIs over launching command-line utilities for core operations.
- Keep UI work on the UI thread and move blocking registry enumeration, file metadata, service, and process queries to bounded worker work where needed. Avoid polling when event-based or explicitly refreshed queries are sufficient.
- Use RAII for handles, COM interfaces, registry keys, and temporary resources. Check API return codes and preserve actionable error details.
- Request elevation only for operations that require it. Explain the action and required privilege before launching an elevated helper. Do not run the entire application elevated by default.
- Never silently change system state. Before a high-impact operation, show the exact target and effect; make a backup or snapshot where supported; verify the result; and provide a restore path.
- Do not store secrets or machine-specific absolute paths in source control.

## UI and Terminology

- Use standard Windows and computer terminology. Do not use invented labels such as “传统动词”. Prefer “经典上下文菜单”, “现代上下文菜单”, “Shell 扩展”, “注册表项”, “服务启动类型”, and “入站/出站规则” where appropriate.
- Display the resolved menu item's friendly name and icon using documented Shell/COM metadata. Registry value names and GUIDs are diagnostic details, not primary labels.
- Distinguish Windows 11 modern context-menu handlers from classic/extended handlers in both data model and UI. Do not claim an entry is active solely because a registry value exists.
- Keep UI responsive, keyboard accessible, DPI-aware, and consistent with the existing application. Keep native idle memory and CPU usage low.
- Any operation that renames, moves, deletes, or changes system configuration must provide a clear preview, confirmation proportional to impact, progress/result feedback, and rollback where feasible.

### UI visual specification

These are the shared native-C++ UI constraints, not a license to introduce a web frontend. Use the existing EUI-NEO components and application state. Put shared theme metrics, icon mapping and reusable page/feedback controls beside the existing `src/app/` code when a page actually needs them; do not create a new UI framework or top-level directory. Numeric geometry, font sizes, colors, radii and shadows must use shared logical-unit tokens instead of repeated literals. Do not double-apply DPI scaling.

#### Layout and visual hierarchy

- Default to a quiet light workbench with a narrow left navigation rail, a compact page header, a scrollable working area, an optional persistent details/inspector panel, and a status bar. Keep scan/refresh/search controls discoverable, but do not reproduce the reference screenshot's pcap tabs, branding or decorative cards. Use compact summary/metric bands only when they show real system data; ordinary pages use a toolbar plus a table/list and details panel.
- Use a logical 4-unit spacing grid. Base sizes: sidebar 208–232, page outer padding 24 (16 when constrained), page header 72–96, table header 36, table row 40, status bar 28. Standard gaps are 4/8/12/16/24; control height is 32 for compact controls, 36 for primary fields and segmented controls. Touch targets remain at least 44 logical units when touch interaction is specifically supported.
- Keep text left-aligned in work areas. Use 1px borders for grouping before shadows; cards/panels use radius 8 (maximum), compact inputs/buttons radius 4–6, menus/dialogs radius 8. Do not nest decorative cards or use large-radius marketing sections. Shadows: subtle panel shadow `(0,2) blur 8 alpha .06`; elevated menu/dialog shadow `(0,6) blur 20 alpha .14`. Never use heavy glow, gradients, bokeh, or continuous decorative animation.

#### Color and typography

- Use these base light tokens: app background `#F3F6FA`; surface `#FFFFFF`; secondary surface `#F6F8FC`; hover `#EDF2F9`; selected `#E7F0FF`; border `#D9E2EF`; primary text `#172033`; secondary text `#526078`; disabled text `#8B97AA`; accent/action `#2563EB`; focus outline `#2563EB`; success `#18794E`; warning `#9A6700`; danger `#C9372C`; info `#1D4ED8`. Semantic status colors need a text/icon label and must not be the only distinction. Use pale semantic backgrounds sparingly and keep primary text contrast at least 4.5:1; verify the rendered result rather than trusting token names.
- Use `Microsoft YaHei UI`/system UI fallback for Chinese UI and `Segoe UI` for Latin; use `Consolas` or the framework's `monospace` family for registry paths, commands and diagnostics. Do not bundle an unrelated display font as the UI font. Base type sizes are 12 caption, 13 body/secondary, 14 control/body, 16 section title, 20 page title, 24–28 key metric; weights are 400 regular, 600 title/strong. Line height is approximately 1.35× the font size; avoid condensed display fonts for data.

#### DPI, input and interaction

- All layout values above are logical units. Use per-monitor DPI awareness (`PerMonitorV2` where supported) and respond to DPI changes, 100/125/150/200% scaling, window resize and minimum-size constraints. Use `screen.width/height` for layout; do not assume a fixed 1280×800 canvas. Ensure no clipped or overlapping text at supported scales.
- Keyboard navigation, visible focus, Enter/Space activation, Escape dismissal, arrow navigation in lists/tabs and accessible names for icon-only controls are required. Provide tooltips for unfamiliar icons; use icons plus text for important commands. Preserve focus after dialogs, support long names with ellipsis and a full-value detail view, and keep expensive Shell/registry/file work off the UI thread.
- Prefer an explicit refresh/action and progress state over polling or fake live counters. Empty, loading, error, unsupported, permission-required and partial-success states must be designed, not improvised. High-impact actions use a preview dialog naming the target/effect, privilege requirement, snapshot and rollback path.

## Local Verification

- The acceptance target is that the application builds, starts, and its delivered core workflows can be used on the developer's local Windows machine.
- Do not add or require end-to-end, browser, or UI automation tests, a VM matrix, or cross-version test infrastructure.
- For each change, run the narrowest applicable build or static check and launch the application locally when the environment supports it. For system-modifying features, verify against a disposable local profile, a reversible fixture, or a read-only/dry-run path before applying real changes.
- Do not create a testing framework or test directory just to satisfy a generic convention. Existing focused unit tests may be used when useful, but they are not a delivery gate.
- Report exactly which local build/run checks were performed and any checks blocked by environment or privilege.

## Change Discipline

- Inspect existing files and `git status` before editing; preserve user changes.
- Keep each change focused on the requested feature. Do not remove or overwrite existing user data.
- Update the canonical development plan and relevant module documentation when behavior or accepted scope changes.
- Before declaring work complete, summarize changed files, local verification, privilege requirements, and remaining limitations.
