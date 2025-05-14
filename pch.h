#pragma once
#define NOMINMAX

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <iostream>
#include <fstream>
#include <stdint.h>
#include <tuple>

#include <drogon/drogon.h>
#include <drogon/HttpController.h>
#include <drogon/WebSocketController.h>
#include <drogon/orm/Mapper.h>
#include <drogon/orm/Result.h>
#include <drogon/orm/Row.h>
#include <drogon/orm/Field.h>
#include <drogon/orm/SqlBinder.h>
#include <drogon/orm/BaseBuilder.h>
#include <drogon/utils/Utilities.h>

#include <trantor/utils/Logger.h>
#include <trantor/utils/Date.h>

#include <json/json.h>

#ifdef _WIN32
#pragma comment(lib, "Rpcrt4.lib")
#pragma comment(lib, "Ws2_32.lib")
#include <rpc.h>
#else
#include <uuid/uuid.h>
#endif

#include "const.h"

#include "Utils.h"
#include "manager/DatabaseManager.h"