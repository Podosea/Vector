MANAGER_PACKAGE_NAME="@MANAGER_PACKAGE_NAME@"
MANAGER_NAMESPACE="@MANAGER_NAMESPACE@"
INJECTED_PACKAGE_NAME="@INJECTED_PACKAGE_NAME@"

# Prefer an installed manager: it owns its own package, uid and launcher icon, so this button
# should bring that one up. Both paths end in the same activity name, and the system matches on
# that name, so whichever copy starts first keeps receiving every later intent — opening the
# installed one here is what keeps the parasitic copy (below) from being the one that wins.
if pm list packages "${MANAGER_PACKAGE_NAME}" 2>/dev/null | grep -qx "package:${MANAGER_PACKAGE_NAME}"; then
  am start -a android.intent.action.MAIN -c android.intent.category.LAUNCHER \
    -n "${MANAGER_PACKAGE_NAME}/${MANAGER_NAMESPACE}.ui.MainActivity"
  exit 0
fi

# Fallback for a device where the manager was never installed: resolve the host's activity and let
# ParasiticManagerSystemHooker rewrite it to the manager, which is injected from the module by fd.
am start -c "${MANAGER_PACKAGE_NAME}.LAUNCH_MANAGER" "${INJECTED_PACKAGE_NAME}/.BugreportWarningActivity"
