#!/system/bin/sh
while [ "$(getprop sys.boot_completed | tr -d '\r')" != "1" ]; do sleep 1; done
sleep 1

API=@API@

MODULE_ID=@MODULE_ID@

MODULE_LIB_NAME=@MODULE_LIB_NAME@

ABI=$(getprop ro.product.cpu.abi)

copy_lib() {
  PACKAGE=$1
  PID=$(dumpsys package "$PACKAGE" | grep userId= | sed 's/[[:blank:]]*userId=//g')
  cp "/data/adb/modules/${API}_${MODULE_ID}/lib/armeabi-v7a/lib${MODULE_LIB_NAME}.so" /data/data/"$PACKAGE"/armeabi-v7a.so
  chown "$PID":"$PID" /data/data/"$PACKAGE"/armeabi-v7a.so
}

copy_lib64() {
  PACKAGE=$1
  PID=$(dumpsys package "$PACKAGE" | grep userId= | sed 's/[[:blank:]]*userId=//g')
  cp "/data/adb/modules/${API}_${MODULE_ID}/lib/arm64-v8a/lib${MODULE_LIB_NAME}.so" /data/data/"$PACKAGE"/arm64-v8a.so
  chown "$PID":"$PID" /data/data/"$PACKAGE"/arm64-v8a.so
  copy_lib "$PACKAGE"
}

get_installed_abi() {
  PACKAGE=$1
  PACKAGE_ABI=$(dumpsys package "$PACKAGE" | grep primaryCpuAbi= | sed 's/[[:blank:]]*primaryCpuAbi=//g')
  echo "$PACKAGE_ABI"
}

if [ "$ABI" = "x86" ]; then
  if [ -d "/data/data/jp.co.cygames.priconnegrandmasters" ]; then
    copy_lib "jp.co.cygames.priconnegrandmasters"
  fi
elif [ $ABI = "x86_64" ]; then
  if [ -d "/data/data/jp.co.cygames.priconnegrandmasters" ]; then
    if get_installed_abi "jp.co.cygames.priconnegrandmasters" = "armeabi-v7a"; then
      copy_lib "jp.co.cygames.priconnegrandmasters"
    else
      copy_lib64 "jp.co.cygames.priconnegrandmasters"
    fi
  fi
fi
