file(REMOVE_RECURSE
  "libaom.a"
  "libaom.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang C)
  include(CMakeFiles/aom.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
