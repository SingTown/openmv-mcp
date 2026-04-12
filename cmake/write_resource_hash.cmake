file(SHA256 ${ZIP} H)
string(SUBSTRING ${H} 0 16 H16)
file(WRITE ${OUT} "#pragma once\n#define OPENMV_RESOURCE_HASH \"${H16}\"\n")
