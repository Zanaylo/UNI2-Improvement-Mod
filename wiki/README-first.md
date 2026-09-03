These pages are the GitHub wiki, kept here so they travel with the source. This folder is the one
to edit; the wiki is published from it.

To publish, copy the pages into the wiki clone and push:

```
copy wiki\*.md ..\UNI2-Improvement-Mod.wiki\
del ..\UNI2-Improvement-Mod.wiki\README-first.md
cd ..\UNI2-Improvement-Mod.wiki
git add . && git commit -m "Wiki" && git push
```

The clone comes from `https://github.com/Zanaylo/UNI2-Improvement-Mod.wiki.git`, and the wiki has to
exist once from the repository's Wiki tab before that works.

`Home.md` is the landing page and `_Sidebar.md` shows on every page. A page called `Installing.md`
is linked as `[Installing](Installing)`, without the extension. This file is not a wiki page.
