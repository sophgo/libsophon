#!/bin/bash

function update_bmcv_commit_and_branch()
{
    file_path=$(find "$(git rev-parse --show-toplevel)" -type f -name "bmcv_internal.cpp" -print -quit)

    if [ -n "$file_path" ]; then
        file_dir=$(dirname "$file_path")
        pushd . > /dev/null

        cd "$file_dir" || exit

        if git rev-parse --git-dir > /dev/null 2>&1; then
            commit_hash=$(git log -1 --pretty=format:"%H")
            branch_name=$(git branch --contains HEAD | sed -n '/\* /s///p')

            sed -i "s|#define COMMIT_HASH .*|#define COMMIT_HASH \"$commit_hash\"|" "bmcv_internal.cpp"
            sed -i "s|#define BRANCH_NAME .*|#define BRANCH_NAME \"$branch_name\"|" "bmcv_internal.cpp"

            echo "Commit hash $commit_hash has been written to $file_path"
            echo "Branch name $branch_name has been written to $file_path"
        else
            echo "This directory is not a git repository."
        fi

        popd > /dev/null
    else
        echo "bmcv_internal.cpp not found."
    fi
}

update_bmcv_commit_and_branch
