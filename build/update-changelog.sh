#!/usr/bin/env bash

changelog=./changelog.md
 
stage_file() {
  file=$1
  if [[ ! -z $file ]]; then
    echo "Staging $file changes..."
    git add $file
    if [[ $(git diff --staged --quiet $file)$? != 1 ]]; then
        echo "$file was not updated"
    fi
  fi
}

# Check out the release branch for changes
git config remote.origin.fetch "+refs/heads/*:refs/remotes/origin/*"
git fetch --all
if [[ ! -z $(git branch -a | grep remotes/origin/release) ]]; then
    echo "Branch 'release' already exists, remove and try again"
    exit 1
fi
git checkout -b release

if [ "$1" = "snapshot" ]; then
    echo "SNAPSHOT"
    new_snapshot=$(echo "$version" | sed -e "s/.*-zepben//g")
    new_version=$(echo "$version" | sed -e "s/zepben.*/zepben$(($new_snapshot+1))/g")
elif [ "$1" = "release" ]; then
    echo "RELEASE"
    new_minor=$(echo $version | sed -e "s/-.*//g" | cut -f4 -d.)
    new_version=$(echo $version | sed -E "s/[[:digit:]]+-zepben/$(($new_minor+1))-zepben/")
else 
    echo "Unsupported mode: '$1'"
fi

echo "Updating changelog to [$new_version]..."
release_notes_template="### Breaking Changes\n* None.\n\n### New Features\n* None.\n\n### Enhancements\n* None.\n\n### Fixes\n* None.\n\n### Notes\n* None.\n\n"
if [[ ! -z $changelog ]]; then
    echo "Timestamping version in changelog..."
    sed -i'' -E "s/UNRELEASED/$(date +'%Y-%m-%d')/g" $changelog

    echo "Inserting template into changelog..."
    sed -i'' "s/\(^# Zepben.*\)/\1\n## \[${new_version}] - UNRELEASED\n$(echo $release_notes_template)/g" $changelog
fi

git config --global user.email "ci@zepben.com"
git config --global user.name "Zepben CI"

# commit to release branch and push
stage_file $changelog
git commit -m "Update versions for release [skip ci]"
git push -u origin release
