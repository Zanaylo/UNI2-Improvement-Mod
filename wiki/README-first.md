These pages are the GitHub wiki. This folder is the source, and the wiki is a copy.

Edit here and commit. `.github/workflows/wiki.yml` publishes the folder to the wiki on every push to
`main` that touches `wiki/`, with the same commit message. There is nothing to run by hand.

A page edited in GitHub's web editor is overwritten by the next publish, and a page deleted here is
deleted there too. Edit in this folder, not on the site.

`Home.md` is the landing page. `_Sidebar.md` shows on every page. A page called `Installing.md` is
linked as `[Installing](Installing)`, without the extension. This file is not a wiki page and is not
copied.

To publish by hand (with the workflow off, or before the first push), open the **Actions** tab, pick
**Wiki** and press **Run workflow**.
