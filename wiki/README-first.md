These pages are the GitHub wiki, kept here so they travel with the source.

To publish them, clone the wiki repository and copy this folder into it:

```
git clone https://github.com/Zanaylo/UNI2-Improvement-Mod.wiki.git
copy wiki\*.md UNI2-Improvement-Mod.wiki\
cd UNI2-Improvement-Mod.wiki
git add .
git commit -m "wiki"
git push
```

The wiki has to be created once from the repository's Wiki tab before that clone works.

`Home.md` is the landing page. A page called `Installing.md` is linked as `[Installing](Installing)`,
without the extension. Delete this file before pushing; it is not a wiki page.
