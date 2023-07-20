Translations
============

The hemis Core project has been designed to support multiple localisations. This makes adding new phrases, and completely new languages easily achievable. For managing all application translations, hemis Core makes use of the Transifex online translation management tool.

### Helping to translate (using Transifex)
Transifex is setup to monitor the GitHub repo for updates, and when code containing new translations is found, Transifex will process any changes. It may take several hours after a pull-request has been merged, to appear in the Transifex web interface.

Multiple language support is critical in assisting hemis's global adoption, and growth. One of hemis's greatest strengths is cross-border money transfers, any help making that easier is greatly appreciated.

See the [Transifex hemis project](https://www.transifex.com/hemis-project/hemis-project-translations/) to assist in translations.

### Writing code with translations
We use automated scripts to help extract translations in both Qt, and non-Qt source files. It is rarely necessary to manually edit the files in `src/qt/locale/`. The translation source files must adhere to the following format:
`hemis_xx_YY.ts or hemis_xx.ts`

`src/qt/locale/hemis_en.ts` is treated in a special way. It is used as the source for all other translations. Whenever a string in the source code is changed, this file must be updated to reflect those changes. A custom script is used to extract strings from the non-Qt parts. This script makes use of `gettext`, so make sure that utility is installed (ie, `apt-get install gettext` on Ubuntu/Debian). Once this has been updated, `lupdate` (included in the Qt SDK) is used to update `hemis_en.ts`.

To automatically regenerate the `hemis_en.ts` file, run the following commands:
```sh
cd src/
make translate
```

`contrib/hemis-qt.pro` takes care of generating `.qm` (binary compiled) files from `.ts` (source files) files. It’s mostly automated, and you shouldn’t need to worry about it.

**Example Qt translation**
```cpp
QToolBar *toolbar = addToolBar(tr("Tabs toolbar"));
```

### Creating a pull-request
For general PRs, you shouldn’t include any updates to the translation source files. They will be updated periodically, primarily around pre-releases, allowing time for any new phrases to be translated before public releases. This is also important in avoiding translation related merge conflicts.

When an updated source file is merged into the GitHub repo, Transifex will automatically detect it (although it can take several hours). Once processed, the new strings will show up as "Remaining" in the Transifex web interface and are ready for translators.

To create the pull-request, use the following commands:
```
git add src/qt/hemisstrings.cpp src/qt/locale/hemis_en.ts
git commit
```

### Creating a Transifex account
Visit the [Transifex Signup](https://www.transifex.com/signup/) page to create an account. Take note of your username and password, as they will be required to configure the command-line tool.

You can find the hemis translation project at [https://www.transifex.com/hemis-project/hemis-project-translations/](https://www.transifex.com/hemis-project/hemis-project-translations/).

### Installing the Transifex client command-line tool
The client is used to fetch updated translations. If you are having problems, or need more details, see [https://docs.transifex.com/client/installing-the-client](https://docs.transifex.com/client/installing-the-client)

`pip install transifex-client`

Setup your Transifex client config as follows. Please *ignore the token field*.

```ini
nano ~/.transifexrc

[https://www.transifex.com]
hostname = https://www.transifex.com
password = PASSWORD
token =
username = USERNAME
```

The Transifex hemis project config file is included as part of the repo. It can be found at `.tx/config`, however you shouldn’t need to change anything.

### Synchronising translations
To assist in updating translations, we have created a script to help.

1. `python contrib/devtools/update-translations.py`
2. `git add` new translations from `src/qt/locale/`
3. Update `src/qt/hemis_locale.qrc` manually or via
```bash
git ls-files src/qt/locale/*ts|xargs -n1 basename|sed 's/\(hemis_\(.*\)\).ts/<file alias="\2">locale\/\1.qm<\/file>/'
```
4. Update `src/Makefile.qt_locale.include` manually or via
```bash
git ls-files src/qt/locale/*ts|xargs -n1 basename|sed 's/\(hemis_\(.*\)\).ts/  qt\/locale\/\1.ts \\/'
```

**Do not directly download translations** one by one from the Transifex website, as we do a few post-processing steps before committing the translations.

### Handling Plurals (in source files)
When new plurals are added to the source file, it's important to do the following steps:

1. Open `hemis_en.ts` in Qt Linguist (included in the Qt SDK)
2. Search for `%n`, which will take you to the parts in the translation that use plurals
3. Look for empty `English Translation (Singular)` and `English Translation (Plural)` fields
4. Add the appropriate strings for the singular and plural form of the base string
5. Mark the item as done (via the green arrow symbol in the toolbar)
6. Repeat from step 2, until all singular and plural forms are in the source file
7. Save the source file

### Translating a new language
To create a new language template, you will need to edit the languages manifest file `src/qt/hemis_locale.qrc` and add a new entry. Below is an example of the English language entry.

```xml
<qresource prefix="/translations">
    <file alias="en">locale/hemis_en.qm</file>
    ...
</qresource>
```

**Note:** that the language translation file **must end in `.qm`** (the compiled extension), and not `.ts`.

### Questions and general assistance
The hemis Core translation maintainers include *Fuzzbawls*. You can find them, and others, in the [hemis Discord](https://discord.hemis.org).

Announcements will be posted during application pre-releases to notify translators to check for updates.
