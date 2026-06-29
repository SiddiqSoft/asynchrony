# GitHub Pages Setup Guide

This guide explains how to set up and configure GitHub Pages for the asynchrony library documentation.

## Prerequisites

- GitHub repository with admin access
- Doxygen installed locally (for testing)
- Git configured on your machine

## Step 1: Enable GitHub Pages

1. Go to your repository on GitHub
2. Click **Settings** → **Pages**
3. Under "Build and deployment":
   - **Source**: Select "GitHub Actions"
   - This allows the workflow to deploy the documentation

## Step 2: Configure Repository Settings

1. Go to **Settings** → **General**
2. Ensure the repository is public (required for free GitHub Pages)
3. Note the repository URL format: `https://github.com/USERNAME/REPO`

## Step 3: Verify Workflow File

The workflow file `.github/workflows/doxygen.yml` should:

1. Trigger on pushes to `main` and `develop` branches
2. Install Doxygen and Graphviz
3. Generate documentation
4. Deploy to GitHub Pages

Current workflow triggers:
- Push to `main` or `develop`
- Changes to `include/`, `docs/`, or `Doxyfile`
- Manual trigger via `workflow_dispatch`

## Step 4: Test Locally

Before pushing, test the documentation generation locally:

```bash
# Install Doxygen and Graphviz
brew install doxygen graphviz  # macOS
# or
sudo apt-get install doxygen graphviz  # Ubuntu

# Generate documentation
doxygen Doxyfile

# View the generated documentation
open docs/doxygen/html/index.html
```

## Step 5: Push Changes

```bash
git add .
git commit -m "Add Doxygen documentation and GitHub Pages setup"
git push origin main
```

## Step 6: Monitor Workflow

1. Go to your repository
2. Click **Actions**
3. Find the "Generate Doxygen Documentation" workflow
4. Monitor the build progress
5. Once complete, the documentation will be deployed

## Step 7: Access Documentation

Once deployed, your documentation will be available at:

```
https://USERNAME.github.io/REPO
```

For example:
```
https://siddiqsoft.github.io/asynchrony
```

## Troubleshooting

### Workflow Not Running

**Problem**: The workflow doesn't appear in the Actions tab

**Solutions**:
1. Ensure the workflow file is in `.github/workflows/` directory
2. Check that the YAML syntax is correct
3. Verify the branch name matches the trigger conditions
4. Try manual trigger: Go to Actions → Select workflow → "Run workflow"

### Build Fails

**Problem**: The workflow fails with an error

**Solutions**:
1. Check the workflow logs for specific errors
2. Verify Doxygen configuration in `Doxyfile`
3. Ensure all input files exist and are readable
4. Check for syntax errors in documentation comments

### Pages Not Deploying

**Problem**: Workflow succeeds but pages don't appear

**Solutions**:
1. Go to Settings → Pages
2. Verify "Source" is set to "GitHub Actions"
3. Check that the artifact is being uploaded correctly
4. Wait a few minutes for GitHub to process the deployment
5. Clear browser cache and try again

### Documentation Not Updating

**Problem**: Changes don't appear in the deployed documentation

**Solutions**:
1. Verify the workflow ran successfully
2. Check that your changes are in the correct branch
3. Ensure the Doxyfile includes your new files
4. Clear browser cache (Ctrl+Shift+Delete or Cmd+Shift+Delete)
5. Wait a few minutes for GitHub to process

## Customization

### Change Deployment Branch

Edit `.github/workflows/doxygen.yml`:

```yaml
on:
  push:
    branches: [ main, develop, custom-branch ]
```

### Change Documentation Output

Edit `Doxyfile`:

```
OUTPUT_DIRECTORY = ./docs/doxygen
```

### Add Custom Domain

1. Go to Settings → Pages
2. Under "Custom domain", enter your domain
3. Add DNS records as instructed by GitHub
4. Enable "Enforce HTTPS"

### Modify Workflow Triggers

Edit `.github/workflows/doxygen.yml` to trigger on:

```yaml
on:
  push:
    branches: [ main ]
    paths:
      - 'include/**'
      - 'docs/**'
      - 'Doxyfile'
  pull_request:
    branches: [ main ]
  schedule:
    - cron: '0 0 * * 0'  # Weekly
  workflow_dispatch:
```

## Advanced Configuration

### Add Status Badge

Add to your README.md:

```markdown
[![Documentation](https://img.shields.io/badge/docs-latest-blue.svg)](https://siddiqsoft.github.io/asynchrony)
```

### Exclude Files from Documentation

Edit `Doxyfile`:

```
EXCLUDE_PATTERNS = */private/* \
                   */test* \
                   */.git/*
```

### Generate Additional Formats

Edit `Doxyfile` to enable:

```
GENERATE_LATEX = YES
GENERATE_MAN = YES
GENERATE_XML = YES
```

### Add Search Functionality

The default Doxygen HTML output includes search. To enable server-based search:

```
SEARCHENGINE = YES
SERVER_BASED_SEARCH = NO
```

## Maintenance

### Regular Updates

1. Keep Doxygen updated in the workflow
2. Review and update documentation regularly
3. Monitor workflow execution times
4. Check for deprecated Doxygen features

### Monitoring

1. Set up email notifications for workflow failures
2. Regularly check the Actions tab
3. Monitor documentation quality
4. Gather user feedback

### Cleanup

Periodically clean up:

1. Remove outdated documentation pages
2. Update broken links
3. Refresh code examples
4. Update API documentation

## Security Considerations

1. **Public Repository**: GitHub Pages requires public repositories for free accounts
2. **Sensitive Information**: Don't include API keys or credentials in documentation
3. **HTTPS**: Always enable HTTPS for your GitHub Pages site
4. **Access Control**: Use branch protection rules to control who can trigger deployments

## Performance Optimization

### Reduce Build Time

1. Exclude unnecessary files in `Doxyfile`
2. Disable unused output formats
3. Reduce graph generation complexity

### Optimize Output Size

1. Disable inline source code
2. Reduce image quality
3. Compress HTML output

## Additional Resources

- [Doxygen Documentation](https://www.doxygen.nl/)
- [GitHub Pages Documentation](https://docs.github.com/en/pages)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [Graphviz Documentation](https://graphviz.org/)

## Support

For issues or questions:

1. Check the troubleshooting section above
2. Review GitHub Actions logs
3. Consult Doxygen documentation
4. Open an issue on the repository
