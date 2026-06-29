# Asynchrony Library Documentation

This directory contains the documentation for the asynchrony library.

## Structure

- **`index.md`** - Main landing page with quick start guide
- **`_config.yml`** - GitHub Pages configuration
- **`pages/`** - Additional documentation pages
  - `mainpage.md` - Doxygen main page
  - `getting_started.md` - Installation and setup guide
  - `usage_guide.md` - Detailed usage examples
  - `examples.md` - Real-world code examples
- **`doxygen/`** - Generated Doxygen documentation (created during build)

## Building Documentation Locally

### Prerequisites

- Doxygen 1.9.8 or later
- Graphviz (for diagram generation)

### Installation

**macOS:**
```bash
brew install doxygen graphviz
```

**Ubuntu/Debian:**
```bash
sudo apt-get install doxygen graphviz
```

**Windows:**
Download from:
- Doxygen: https://www.doxygen.nl/download.html
- Graphviz: https://graphviz.org/download/

### Generate Documentation

From the project root directory:

```bash
doxygen Doxyfile
```

The generated HTML documentation will be in `docs/doxygen/html/`.

### View Documentation

Open the generated documentation in your browser:

```bash
open docs/doxygen/html/index.html  # macOS
xdg-open docs/doxygen/html/index.html  # Linux
start docs/doxygen/html/index.html  # Windows
```

## GitHub Pages Deployment

The documentation is automatically built and deployed to GitHub Pages when changes are pushed to the `main` or `develop` branches.

### Workflow

The `.github/workflows/doxygen.yml` workflow:
1. Installs Doxygen and Graphviz
2. Generates the documentation
3. Deploys to GitHub Pages

### Accessing Online Documentation

Visit: https://siddiqsoft.github.io/asynchrony

## Documentation Guidelines

### Adding New Pages

1. Create a new `.md` file in the `docs/pages/` directory
2. Start with a Doxygen page comment:
   ```cpp
   /**
    * @page page_name Page Title
    * 
    * Your content here...
    */
   ```
3. The page will be automatically included in the generated documentation

### Code Examples

Use code blocks with language specification:

````markdown
```cpp
#include "siddiqsoft/simple_worker.hpp"

int main() {
    // Your code here
}
```
````

### Cross-References

Link to other pages and classes:

```markdown
- @ref siddiqsoft::simple_worker
- @ref getting_started
- @ref usage_guide
```

### Doxygen Commands

Common Doxygen commands:

- `@brief` - Brief description
- `@param` - Parameter documentation
- `@return` - Return value documentation
- `@throws` - Exception documentation
- `@note` - Important note
- `@warning` - Warning message
- `@see` - See also reference
- `@example` - Example code

## Customization

### Modifying Doxyfile

Edit the `Doxyfile` to customize:
- Project name and version
- Output directory
- Input files and patterns
- HTML styling
- Diagram generation

Key settings:

```
PROJECT_NAME = "asynchrony"
PROJECT_NUMBER = 0.10
OUTPUT_DIRECTORY = ./docs/doxygen
GENERATE_HTML = YES
HAVE_DOT = YES
```

### Styling

To customize the HTML output:

1. Generate a custom header/footer:
   ```bash
   doxygen -w html header.html footer.html style.css Doxyfile
   ```

2. Edit the generated files

3. Update Doxyfile:
   ```
   HTML_HEADER = header.html
   HTML_FOOTER = footer.html
   HTML_STYLESHEET = style.css
   ```

## Troubleshooting

### Doxygen not found

Ensure Doxygen is installed and in your PATH:

```bash
which doxygen
doxygen --version
```

### Graphviz not found

Doxygen will generate documentation without Graphviz, but diagrams won't be created.

Install Graphviz and ensure `dot` is in your PATH:

```bash
which dot
dot -V
```

### Documentation not updating

1. Clear the build directory:
   ```bash
   rm -rf docs/doxygen
   ```

2. Regenerate:
   ```bash
   doxygen Doxyfile
   ```

### GitHub Pages not deploying

1. Check the workflow status in GitHub Actions
2. Ensure GitHub Pages is enabled in repository settings
3. Set the source to "GitHub Actions"

## Contributing

When contributing documentation:

1. Follow the existing style and format
2. Use clear, concise language
3. Include code examples where appropriate
4. Test locally before submitting
5. Update the table of contents if adding new pages

## License

Documentation is part of the asynchrony library and is covered under the same BSD 3-Clause License.

See LICENSE file for details.
