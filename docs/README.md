# SVD Documentation Contributor Guide

This directory contains the source files for the official SVD documentation website. The site is built using **Quarto**, a modern open-source scientific and technical publishing system.

---

## 1. Setup and Local Preview

### Prerequisites
Make sure you have Quarto installed. You can download it from [quarto.org](https://quarto.org).

### Previewing the Site Locally
To run a local live-reload preview server while editing `.qmd` files:
```bash
# From within the docs/ directory:
quarto preview
```
This will spin up a local server (typically at `http://localhost:4739/`) that automatically refreshes your browser when you save any document.

### Rendering the Site
To compile the entire site into static HTML assets:
```bash
# Renders the website to the `docs/_site/` folder
quarto render
```

### Publishing to GitHub Pages
To publish the documentation to GitHub Pages:
```bash
# 1. Change to the docs/ directory where the Quarto source files are located
cd docs

# 2. Run the publish command
quarto publish gh-pages
```

#### What happens in the background:
When you run `quarto publish gh-pages`, Quarto performs the following steps automatically:
1. **Renders the site**: It compiles all `.qmd` source files into a temporary directory containing the final static HTML, CSS, JS, and image assets.
2. **Updates the `gh-pages` branch**: It commits the rendered static assets directly to a dedicated local `gh-pages` branch (creating it first if it does not exist).
3. **Pushes to GitHub**: It pushes the local `gh-pages` branch to the remote repository on GitHub, which triggers GitHub's automated build and deployment process to publish the site live.

---

## 2. Structure of the Source Files

*   **`_quarto.yml`**: The main configuration file containing metadata, navigation bar settings, sidebar outline structure, and theme customizations.
*   **`index.qmd`**: The main hub page.
*   **`custom.css`**: Global CSS styling adjustments (light/dark general layout rules).
*   **`custom-light.scss`**: Sass variables (like primary brand colors) for the light theme profile.
*   **`custom-dark.scss`**: Sass variables and CSS rules tailored specifically for the dark theme profile.
*   **`img/`**: Stores images, screenshots, and brand assets (including `svd_logo_light.png` and `svd_logo_dark.png`).
*   **Other `.qmd` files**: Individual documentation pages organized under sidebar sections.

---

## 3. Generating C++ Output Descriptions (`outputs.qmd`)

The details of SVD outputs (database tables, columns, and data types) are maintained directly within the C++ source code of the model. 

To update the outputs page ([outputs.qmd](outputs.qmd)) with changes made in the code:

1.  Ensure you are running a build of SVD compiled from the full repository source.
2.  Open the SVD GUI application (`SVDUI`).
3.  Trigger the output description compilation from the application menu:
    *   Go to **Help** / **Create Output Descriptions** (or trigger the slot `MainWindow::on_actioncreate_output_docs_triggered()`).
4.  The application will automatically locate `docs/outputs.qmd` relative to the build directory and write the generated outputs documentation block directly between the comment boundaries:
    ```html
    <!-- GENERATED-CODE-START -->
    ... (automatically generated table structures from C++ output definitions) ...
    <!-- GENERATED-CODE-END -->
    ```
5.  After the execution completes, review the diff in `outputs.qmd` and run `quarto render` to compile the changes to the static site.

---

## 4. Managing Documentation Versions

We support multi-version documentation hosting to allow users to view current, historical, or development-level features.

### Deployment Directory Structure on the Web Server:
All versions are hosted under separate subfolders on the server:
```text
/var/www/svd-docs/
├── latest/ -> 1.0/            # A symlink pointing to the current stable folder (e.g. /1.0/)
├── dev/                       # Bleeding-edge build from the master/main branch
└── 1.0/                       # Frozen archive of version 1.0 documentation
```

### The Version Switcher Dropdown
The version selector dropdown can be defined in `_quarto.yml` under `website.navbar.right` if dropdown support is needed. 

### How to Release/Freeze a New Documentation Version (e.g., v1.1):
1.  **Branch off release:** Create a git branch `v1.1` from the current development state (`master` / `main`).
2.  **Update configurations:** 
    *   On the new release branch, modify `_quarto.yml` if necessary (e.g., to reference the frozen version or specific theme details).
3.  **Compile:** Render the documentation on the release branch:
    ```bash
    quarto render
    ```
4.  **Upload:** Deploy the resulting `_site` folder to the `/1.1/` subdirectory on the web server.
5.  **Redirect Stable:** Update the `latest` symlink on the web server to point to `/1.1/`.
