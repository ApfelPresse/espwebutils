#pragma once

#include <Arduino.h>

// Umbrella header: keeps the public API stable while splitting implementation
// into focused headers under src/model/var/.

#include "ModelSerializer.h"
#include "types/ModelTypeTraits.h"

#include "var/VarPolicy.h"
#include "var/VarTraits.h"
#include "var/VarJsonDispatch.h"
#include "var/Var.h"
#include "var/VarAliases.h"
#include "var/VarFieldIo.h"
