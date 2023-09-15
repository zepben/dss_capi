#!/usr/bin/env bash

changelog=./changelog.md
 
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

# Check that the provided tag doesn't exist yet
check_tag_exists $1

# Check out the release branch for changes
git config remote.origin.fetch "+refs/heads/*:refs/remotes/origin/*"
git fetch --all
if [[ ! -z $(git branch -a | grep remotes/origin/release) ]]; then
    echo "Branch 'release' already exists, remove and try again"
    exit 1
fi
git checkout -b release

new_minor=$(echo $version | sed -e "s/-.*//g" | cut -f4 -d.)
new_version=$(echo $version | sed -e "s/[[:digit:]]-zepben/$new_minor/g")

release_notes_template="### Breaking Changes\n* None.\n\n### New Features\n* None.\n\n### Enhancements\n* None.\n\n### Fixes\n* None.\n\n### Notes\n* None.\n\n"
if [[ ! -z $changelog ]]; then
    echo "Timestamping version in changelog..."
    sed -i'' -E "s/UNRELEASED/$(date +'%Y-%m-%d')/g" $changelog

    echo "Inserting template into changelog..."
    sed -i'' "s/\(^# Zepben.*\)/\1\n## \[${new_version}-zepben] - UNRELEASED\n$(echo $release_notes_template)/g" $changelog
fi

# commit to release branch and push
stage_file $changelog
git commit -m "Update versions for release [skip ci]"
git push -u origin release
