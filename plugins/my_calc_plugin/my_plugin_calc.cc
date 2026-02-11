#ifndef MYSQL_DYNAMIC_PLUGIN
#define MYSQL_DYNAMIC_PLUGIN
#endif

#include <mysql/plugin.h>
#include <mysql.h>
#include <string.h>

// UDF関数の宣言（C リンケージ）
extern "C" {
    bool mycalc_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
    longlong mycalc(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error);
    void mycalc_deinit(UDF_INIT *initid);
}

// システム変数の実体
static bool mycalc_is_minus = false;

// システム変数の定義
static MYSQL_SYSVAR_BOOL(
    is_minus,
    mycalc_is_minus,
    PLUGIN_VAR_OPCMDARG,
    "If true, mycalc performs subtraction instead of addition.",
    NULL, NULL, false
);

static struct st_mysql_sys_var *my_system_vars[] = {
    (struct st_mysql_sys_var *)MYSQL_SYSVAR(is_minus),
    NULL
};

// UDF関数の実装
bool mycalc_init(UDF_INIT *initid, UDF_ARGS *args, char *message) {
    if (args->arg_count != 2) {
        strcpy(message, "mycalc() requires exactly 2 arguments");
        return true;
    }
    return false;
}

longlong mycalc(UDF_INIT *initid, UDF_ARGS *args, char *is_null, char *error) {
    longlong a = *((longlong *)args->args[0]);
    longlong b = *((longlong *)args->args[1]);
    return mycalc_is_minus ? (a - b) : (a + b);
}

void mycalc_deinit(UDF_INIT *initid) {}

// ★ デーモンプラグイン用のインターフェース構造体
static struct st_mysql_daemon my_daemon_handler = {
    MYSQL_DAEMON_INTERFACE_VERSION
};

// プラグイン宣言
mysql_declare_plugin(my_calc_plugin)
{
    MYSQL_DAEMON_PLUGIN,                    // ★ DAEMON に変更
    &my_daemon_handler,                     // ★ info 構造体を指定
    "my_calc_plugin",
    "Your Name",
    "A calculator plugin with a toggle switch",
    PLUGIN_LICENSE_GPL,
    NULL,                                   // init
    NULL,                                   // check_uninstall
    NULL,                                   // deinit
    0x0100,
    NULL,
    (SYS_VAR **)my_system_vars,
    NULL,
    0,
}
mysql_declare_plugin_end;
