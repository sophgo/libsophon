function lfs_pull()
{
 git lfs pull --include=$1 --exclude=""
}

function lfs_install()
{
  git lfs install
}

export GIT_SSL_NO_VERIFY=1
export GIT_LFS_SKIP_SMUDGE=1
