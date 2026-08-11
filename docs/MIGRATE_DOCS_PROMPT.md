# Gemini CLI Instructions: Migrate Documentation to MkDocs (Material Theme)

This document contains step-by-step instructions for Gemini CLI to migrate documentation on any C++ repository to **MkDocs** using the **Material for MkDocs (`mkdocs-material`)** theme, matching the layout and UI of [https://json.nlohmann.me/](https://json.nlohmann.me/).

---

## Prompt for Gemini CLI

```markdown
I want to migrate the documentation of this repository from Doxygen/Jekyll to MkDocs using the Material theme (`mkdocs-material`), matching the UI, color scheme, and layout of https://json.nlohmann.me/. Please execute the following steps:

### 1. Create `mkdocs.yml` Configuration
Create `mkdocs.yml` in the root directory with the following configuration:
- Site name, description, author, repository URL, edit URI.
- `theme: material` with indigo primary and accent colors (`primary: indigo`, `accent: indigo`).
- Use the logo in pack/Siddiq-Software-Avatar.png as the logo.
- Light/Dark mode palette toggle (`scheme: default` for light, `scheme: slate` for dark).
- Features enabled:
  - `navigation.tabs`, `navigation.tabs.sticky`, `navigation.sections`, `navigation.expand`, `navigation.path`, `navigation.top`, `navigation.footer`, `navigation.tracking`
  - `search.suggest`, `search.highlight`, `search.share`
  - `content.code.copy`, `content.code.annotate`
- Markdown extensions: `admonition`, `pymdownx.details`, `pymdownx.superfences`, `pymdownx.highlight` (with line numbers & pygments), `pymdownx.inlinehilite`, `pymdownx.snippets`, `pymdownx.tabbed` (alternate style), `attr_list`, `md_in_html`, `tables`, `toc` (permalink).
- Custom CSS: `docs/css/custom.css`.
- Navigation structure mapping to markdown pages in `docs/` (`Home`, `Features`, `Integration`, `API Reference`, `License`).

### 2. Create Custom Stylesheet
Create `docs/css/custom.css` with font overrides (`Roboto` for body text and `JetBrains Mono` for code blocks), table shadows/borders, badge containers, and code/admonition styling.

### 3. Restructure and Port Markdown Pages
Organize `docs/` into clean, modular pages:
- `docs/index.md`: Overview, status badges, design goals, tabbed quick start examples, requirements table, and navigation links.
- `docs/features/`: Feature overview and topic-specific guides.
- `docs/integration/`: CMake / submodule / package manager guides.
- `docs/api/`: API overview directory and class/function reference pages.
- `docs/license.md`: Project license text.

### 4. Create Local Tooling & Requirements
- `docs/requirements.txt`:
  ```text
  mkdocs>=1.6.0
  mkdocs-material>=9.5.0
  pymdown-extensions>=10.0
  ```
- `docs/rebuild-docs.ps1` and `docs/rebuild-docs.sh`: Local build scripts executing `mkdocs build --config-file ../mkdocs.yml`.

### 5. Update Azure pipelines to publish to github pages
- Trigger on `push` and `pull_request` to `main`/`master` branches.
- Set permissions for GitHub Pages (`pages: write`, `id-token: write`).
- Set up Python 3, install `docs/requirements.txt`, build with `mkdocs build --strict`.
- Deploy to GitHub Pages 
- Create `.azure/az-publish-docs.yml` to build MkDocs and publish to the `gh-pages` branch using `ghp-import`.
- Update `azure-pipelines.yml` to include a `PublishDocs` parameter and job step under the `Publication` stage.

### 6. Remove Obsolete Doxygen / Jekyll Files
Delete obsolete documentation files:
- `docs/Doxyfile`, `docs/doxygen-awesome*`, `docs/header.html`, `docs/footer.html`, `docs/mainpage.md`, `docs/API.md`, `docs/_config.yml`.
- Remove any lingering references to Doxygen in `README.md` and repository build scripts.
- Add `site/` and `.venv/` to `.gitignore`.

### 7. Verify
Execute `mkdocs build --strict` to ensure zero build errors or warnings.
```
