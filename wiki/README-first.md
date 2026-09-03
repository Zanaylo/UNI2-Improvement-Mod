These pages are the GitHub wiki. This folder is the source; the wiki is a copy of it.

Edit here and commit. `.github/workflows/wiki.yml` publishes the folder to the wiki on every push to
`main` that touches `wiki/`, under the same commit message. Nothing to run by hand.

A page edited in GitHub's web editor is overwritten by the next publish, and a page deleted here is
deleted there. Edit in this folder, not on the site.

`Home.md` is the landing page and `_Sidebar.md` shows on every page. A page called `Installing.md`
is linked as `[Installing](Installing)`, without the extension. This file is not a wiki page and is
not copied.

To publish by hand - the workflow off, or before the first push - the **Actions** tab has **Wiki**
with a **Run workflow** button.
