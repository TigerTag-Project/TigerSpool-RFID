#pragma once

// The ONE source of truth for the firmware version.
//
// The release workflow reads this macro and refuses to publish if the git tag
// disagrees with it, and scripts/bump-version.sh is the only thing that should
// edit it. Two places to change a version is one place to forget.
//
// Semantic versioning: MAJOR.MINOR.PATCH.
#define TIGERSPOOL_FW_VERSION "1.23.0"
