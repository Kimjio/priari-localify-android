#pragma once

#ifndef PRIARILOCALIFYANDROID_GAME_HPP
#define PRIARILOCALIFYANDROID_GAME_HPP

#include <string>

#define Unity2019 "2019.4.15f1"

using namespace std;

namespace Game {
    enum class Region {
        UNKNOWN,
        JAP,
        KOR,
    };

    enum class Store {
        Google,
    };

    inline auto currentGameRegion = Region::UNKNOWN;
    inline auto currentGameStore = Store::Google;

    inline auto GamePackageName = "jp.co.cygames.priconnegrandmasters"s;
    inline auto GamePackageNameKor = "com.kakaogames.pcrgm"s;

    static bool IsPackageNameEqualsByGameRegion(const char *pkgNm, Region gameRegion) {
        string pkgNmStr = string(pkgNm);
        if (pkgNmStr.empty()) {
            return false;
        }
        switch (gameRegion) {
            case Region::JAP:
                if (pkgNmStr == GamePackageName) {
                    currentGameRegion = Region::JAP;
                    currentGameStore = Store::Google;
                    return true;
                }
                break;
            case Region::KOR:
                if (pkgNmStr == GamePackageNameKor) {
                    currentGameRegion = Region::KOR;
                    currentGameStore = Store::Google;
                    return true;
                }
                break;
            case Region::UNKNOWN:
            default:
                break;
        }
        return false;
    }

    static string GetPackageNameByGameRegionAndGameStore(Region gameRegion, Store gameStore) {
        if (gameRegion == Region::JAP)
            return GamePackageName;
        if (gameRegion == Region::KOR)
            return GamePackageNameKor;
        return "";
    }

    static string GetCurrentPackageName() {
        return GetPackageNameByGameRegionAndGameStore(currentGameRegion, currentGameStore);
    }

    static Region CheckPackageNameByDataPath() {
        if (access(
                "/data/data/"s
                        .append(GetPackageNameByGameRegionAndGameStore(Region::JAP,
                                                                       Store::Google)).append(
                        "/cache").data(),
                F_OK) == 0) {
            return Region::JAP;
        }
        if (access(
                "/data/data/"s
                        .append(GetPackageNameByGameRegionAndGameStore(Region::KOR,
                                                                       Store::Google)).append(
                        "/cache").data(),
                F_OK) == 0) {
            return Region::KOR;
        }

        return Region::UNKNOWN;
    }
}

#endif
