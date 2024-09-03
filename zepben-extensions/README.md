# How to update from upstream

When looking to update dss_capi versions the process is:

1. Add the upstream remote

        git remote add upstream git@github.com:dss-extensions/dss_capi.git

2. Create a new branch off the current main

        git checkout -b DEV-XXX-update-from-dss-capi-<tag>

3. Perform a merge from dss-capi into your new branch

        git fetch upstream --tags

4. Merge the relevant tag into your branch (NOTE: this command has not been tested - plz talk to kurt before this step)

        git merge upstream/<tag>

5. Resolve conflicts
6. Test
8. Do a release/tagging of the current main branch. The github action "Build Zepben Linux Libraries" should do this for you.
Make sure the tag is applied before you do the next steps!
7. Update the changelog in your branch to reflect the new version (e.g `<dss_capi_version>-zepben1`), with a new section at the top.
8. Force push your branch to main:

        git push --force origin <your-branch>:main

Your branch now becomes the new main and you can delete it.

    git checkout main
    git pull
    git branch -D <your-branch>

