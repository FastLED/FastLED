#!/bin/bash

if [ "$RENDER" != "true" ]; then
  echo "Skipping finalprewarm..."
  exit 0
fi

git_path=/git/fastled
fastled_path=/js/fastled

# update the fastled git repo
cd $git_path

git fetch origin
git reset --hard origin/master
#  ["rsync", "-av", "--info=NAME", "--delete", f"{src}/", f"{dst}/"],

cd /js

rsync -av --info=NAME --delete "$git_path/" "$fastled_path/"  --exclude ".git"
# rsync -av --info=NAME --delete "$git_path/examples" "$fastled_path/src"

# now print out all the files in the current directory
ls -la


