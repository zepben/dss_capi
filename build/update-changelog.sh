#!/usr/bin/env bash

changelog=./changelog.md
version=$(grep -E "## \[[0-9]+\.[0-9]+\.[0-9]\.[0-9]+-zepben\]" $changelog | grep UNRELE | cut -f2 -d' ')
 
check_tag_exists() {
  version=${1:? 'Version variable is missing.'}
  echo "Checking remote tags if version exists..."
  git config remote.origin.fetch "+refs/heads/*:refs/remotes/origin/*"
  git fetch --tags origin
  old_tag=$(git tag -l | grep "^$version$" || true)
  tag=$(git tag -l | grep "^v$version$" || true)
  if [[ ! -z $tag || ! -z $old_tag ]]; then
      fail "Tag for this version already exists"
  fi
}

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

# Check that this version doesn't exist yet
check_tag_exists $version

# Checkout the release branch
git checkout -b release
if [ $? != 0 ]; then
    echo "Couldn't checkout the release branch, please check if it exists and remove if needed"
fi

release_notes_template="### Breaking Changes\n* None.\n\n### New Features\n* None.\n\n### Enhancements\n* None.\n\n### Fixes\n* None.\n\n### Notes\n* None.\n\n"
if [[ ! -z $changelog ]]; then
    echo "Timestamping version in changelog..."
    sed -i'' -E "s/UNRELEASED/$(date +'%Y-%m-%d')/g" $changelog

    echo "Inserting template into changelog..."
    sed -i'' "s/\(^# Zepben.*\)/\1\n## \[NEW VERSION HERE-zepben] - UNRELEASED\n$(echo $release_notes_template)/g" $changelog
fi

# commit to release branch and push
git add $changelog
git commit -m "Update versions for release [skip ci]"
git push -u origin release

# 
