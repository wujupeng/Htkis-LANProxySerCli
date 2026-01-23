#include "Localization.h"

Localization& Localization::getInstance() {
    static Localization instance;
    return instance;
}

Localization::Localization() {
    m_current_lang = Language::EN;

    m_languages.push_back({Language::EN, "English", ""});
    m_languages.push_back({Language::ZH_CN, "简体中文", "PingFang.ttc"});
    m_languages.push_back({Language::VI, "Tiếng Việt", "Arial Unicode.ttf"});
    m_languages.push_back({Language::TH, "ไทย", "Thonburi.ttc"});
    m_languages.push_back({Language::ES_MX, "Español (México)", ""});
    m_languages.push_back({Language::HU, "Magyar", ""});

    // English
    m_translations[Language::EN] = std::unordered_map<std::string, std::string>();
    m_translations[Language::EN]["APP_TITLE_SERVER"] = "Htkis-LANProxySerCli - Server";
    m_translations[Language::EN]["APP_TITLE_CLIENT"] = "Htkis-LANProxySerCli - Client";
    m_translations[Language::EN]["USER_MANAGEMENT"] = "User Management";
    m_translations[Language::EN]["USERNAME"] = "Username";
    m_translations[Language::EN]["PASSWORD"] = "Password";
    m_translations[Language::EN]["DAYS"] = "Days";
    m_translations[Language::EN]["ADD_USER"] = "Add User";
    m_translations[Language::EN]["REFRESH_LIST"] = "Refresh List";
    m_translations[Language::EN]["ACTIVE"] = "Active";
    m_translations[Language::EN]["EXPIRE_TIME"] = "Expire Time";
    m_translations[Language::EN]["ACTIONS"] = "Actions";
    m_translations[Language::EN]["DEACTIVATE"] = "Deactivate";
    m_translations[Language::EN]["ACTIVATE"] = "Activate";
    m_translations[Language::EN]["DELETE"] = "Delete";
    m_translations[Language::EN]["SERVER_IP"] = "Server IP";
    m_translations[Language::EN]["SERVER_PORT"] = "Server Port";
    m_translations[Language::EN]["LOCAL_PORT"] = "Local Port";
    m_translations[Language::EN]["CONNECT"] = "Connect";
    m_translations[Language::EN]["DISCONNECT"] = "Disconnect";
    m_translations[Language::EN]["STATUS_RUNNING"] = "RUNNING";
    m_translations[Language::EN]["STATUS_STOPPED"] = "STOPPED";
    m_translations[Language::EN]["STATUS_CONNECTED"] = "CONNECTED / RUNNING";
    m_translations[Language::EN]["STATUS_DISCONNECTED"] = "DISCONNECTED";
    m_translations[Language::EN]["ABOUT"] = "About";
    m_translations[Language::EN]["DEVELOPER"] = "Developer";
    m_translations[Language::EN]["EMAIL"] = "Email";
    m_translations[Language::EN]["LANGUAGE"] = "Language";
    m_translations[Language::EN]["YES"] = "Yes";
    m_translations[Language::EN]["NO"] = "No";
    m_translations[Language::EN]["START_SERVER"] = "Start Server";
    m_translations[Language::EN]["STOP_SERVER"] = "Stop Server";
    m_translations[Language::EN]["SERVER_CONTROL_PANEL"] = "Server Control Panel";
    m_translations[Language::EN]["CLIENT_CONNECTION"] = "Client Connection";
    m_translations[Language::EN]["CLIENT_HINT"] = "Configure your system or browser proxy to SOCKS5 127.0.0.1:";

    // Simplified Chinese
    m_translations[Language::ZH_CN] = std::unordered_map<std::string, std::string>();
    m_translations[Language::ZH_CN]["APP_TITLE_SERVER"] = "Htkis-LANProxySerCli - 服务端";
    m_translations[Language::ZH_CN]["APP_TITLE_CLIENT"] = "Htkis-LANProxySerCli - 客户端";
    m_translations[Language::ZH_CN]["USER_MANAGEMENT"] = "用户管理";
    m_translations[Language::ZH_CN]["USERNAME"] = "用户名";
    m_translations[Language::ZH_CN]["PASSWORD"] = "密码";
    m_translations[Language::ZH_CN]["DAYS"] = "有效天数";
    m_translations[Language::ZH_CN]["ADD_USER"] = "添加用户";
    m_translations[Language::ZH_CN]["REFRESH_LIST"] = "刷新列表";
    m_translations[Language::ZH_CN]["ACTIVE"] = "状态";
    m_translations[Language::ZH_CN]["EXPIRE_TIME"] = "过期时间";
    m_translations[Language::ZH_CN]["ACTIONS"] = "操作";
    m_translations[Language::ZH_CN]["DEACTIVATE"] = "停用";
    m_translations[Language::ZH_CN]["ACTIVATE"] = "启用";
    m_translations[Language::ZH_CN]["DELETE"] = "删除";
    m_translations[Language::ZH_CN]["SERVER_IP"] = "服务器 IP";
    m_translations[Language::ZH_CN]["SERVER_PORT"] = "服务器端口";
    m_translations[Language::ZH_CN]["LOCAL_PORT"] = "本地端口";
    m_translations[Language::ZH_CN]["CONNECT"] = "连接";
    m_translations[Language::ZH_CN]["DISCONNECT"] = "断开连接";
    m_translations[Language::ZH_CN]["STATUS_RUNNING"] = "运行中";
    m_translations[Language::ZH_CN]["STATUS_STOPPED"] = "已停止";
    m_translations[Language::ZH_CN]["STATUS_CONNECTED"] = "已连接 / 运行中";
    m_translations[Language::ZH_CN]["STATUS_DISCONNECTED"] = "未连接";
    m_translations[Language::ZH_CN]["ABOUT"] = "关于";
    m_translations[Language::ZH_CN]["DEVELOPER"] = "开发者";
    m_translations[Language::ZH_CN]["EMAIL"] = "联系邮箱";
    m_translations[Language::ZH_CN]["LANGUAGE"] = "语言";
    m_translations[Language::ZH_CN]["YES"] = "是";
    m_translations[Language::ZH_CN]["NO"] = "否";
    m_translations[Language::ZH_CN]["START_SERVER"] = "启动服务";
    m_translations[Language::ZH_CN]["STOP_SERVER"] = "停止服务";
    m_translations[Language::ZH_CN]["SERVER_CONTROL_PANEL"] = "服务端控制面板";
    m_translations[Language::ZH_CN]["CLIENT_CONNECTION"] = "客户端连接";
    m_translations[Language::ZH_CN]["CLIENT_HINT"] = "请配置您的系统或浏览器代理为 SOCKS5 127.0.0.1:";

    // Vietnamese
    m_translations[Language::VI] = std::unordered_map<std::string, std::string>();
    m_translations[Language::VI]["APP_TITLE_SERVER"] = "Htkis-LANProxySerCli - Máy chủ";
    m_translations[Language::VI]["APP_TITLE_CLIENT"] = "Htkis-LANProxySerCli - Máy khách";
    m_translations[Language::VI]["USER_MANAGEMENT"] = "Quản lý người dùng";
    m_translations[Language::VI]["USERNAME"] = "Tên người dùng";
    m_translations[Language::VI]["PASSWORD"] = "Mật khẩu";
    m_translations[Language::VI]["DAYS"] = "Ngày";
    m_translations[Language::VI]["ADD_USER"] = "Thêm người dùng";
    m_translations[Language::VI]["REFRESH_LIST"] = "Làm mới danh sách";
    m_translations[Language::VI]["ACTIVE"] = "Hoạt động";
    m_translations[Language::VI]["EXPIRE_TIME"] = "Thời gian hết hạn";
    m_translations[Language::VI]["ACTIONS"] = "Hành động";
    m_translations[Language::VI]["DEACTIVATE"] = "Hủy kích hoạt";
    m_translations[Language::VI]["ACTIVATE"] = "Kích hoạt";
    m_translations[Language::VI]["DELETE"] = "Xóa";
    m_translations[Language::VI]["SERVER_IP"] = "IP Máy chủ";
    m_translations[Language::VI]["SERVER_PORT"] = "Cổng Máy chủ";
    m_translations[Language::VI]["LOCAL_PORT"] = "Cổng cục bộ";
    m_translations[Language::VI]["CONNECT"] = "Kết nối";
    m_translations[Language::VI]["DISCONNECT"] = "Ngắt kết nối";
    m_translations[Language::VI]["STATUS_RUNNING"] = "ĐANG CHẠY";
    m_translations[Language::VI]["STATUS_STOPPED"] = "ĐÃ DỪNG";
    m_translations[Language::VI]["STATUS_CONNECTED"] = "ĐÃ KẾT NỐI";
    m_translations[Language::VI]["STATUS_DISCONNECTED"] = "NGẮT KẾT NỐI";
    m_translations[Language::VI]["ABOUT"] = "Giới thiệu";
    m_translations[Language::VI]["DEVELOPER"] = "Nhà phát triển";
    m_translations[Language::VI]["EMAIL"] = "Email";
    m_translations[Language::VI]["LANGUAGE"] = "Ngôn ngữ";
    m_translations[Language::VI]["YES"] = "Có";
    m_translations[Language::VI]["NO"] = "Không";
    m_translations[Language::VI]["START_SERVER"] = "Bắt đầu";
    m_translations[Language::VI]["STOP_SERVER"] = "Dừng lại";
    m_translations[Language::VI]["SERVER_CONTROL_PANEL"] = "Bảng điều khiển máy chủ";
    m_translations[Language::VI]["CLIENT_CONNECTION"] = "Kết nối máy khách";
    m_translations[Language::VI]["CLIENT_HINT"] = "Cấu hình proxy trình duyệt hoặc hệ thống của bạn thành SOCKS5 127.0.0.1:";
    
    // Thai
    m_translations[Language::TH] = std::unordered_map<std::string, std::string>();
    m_translations[Language::TH]["APP_TITLE_SERVER"] = "Htkis-LANProxySerCli - เซิร์ฟเวอร์";
    m_translations[Language::TH]["APP_TITLE_CLIENT"] = "Htkis-LANProxySerCli - ลูกค้า";
    m_translations[Language::TH]["USER_MANAGEMENT"] = "การจัดการผู้ใช้";
    m_translations[Language::TH]["USERNAME"] = "ชื่อผู้ใช้";
    m_translations[Language::TH]["PASSWORD"] = "รหัสผ่าน";
    m_translations[Language::TH]["DAYS"] = "วัน";
    m_translations[Language::TH]["ADD_USER"] = "เพิ่มผู้ใช้";
    m_translations[Language::TH]["REFRESH_LIST"] = "รีเฟรชรายการ";
    m_translations[Language::TH]["ACTIVE"] = "คล่องแคล่ว";
    m_translations[Language::TH]["EXPIRE_TIME"] = "หมดเวลา";
    m_translations[Language::TH]["ACTIONS"] = "การกระทำ";
    m_translations[Language::TH]["DEACTIVATE"] = "ปิดใช้งาน";
    m_translations[Language::TH]["ACTIVATE"] = "เปิดใช้งาน";
    m_translations[Language::TH]["DELETE"] = "ลบ";
    m_translations[Language::TH]["SERVER_IP"] = "IP เซิร์ฟเวอร์";
    m_translations[Language::TH]["SERVER_PORT"] = "พอร์ตเซิร์ฟเวอร์";
    m_translations[Language::TH]["LOCAL_PORT"] = "พอร์ตท้องถิ่น";
    m_translations[Language::TH]["CONNECT"] = "เชื่อมต่อ";
    m_translations[Language::TH]["DISCONNECT"] = "ตัดการเชื่อมต่อ";
    m_translations[Language::TH]["STATUS_RUNNING"] = "กำลังทำงาน";
    m_translations[Language::TH]["STATUS_STOPPED"] = "หยุด";
    m_translations[Language::TH]["STATUS_CONNECTED"] = "เชื่อมต่อแล้ว";
    m_translations[Language::TH]["STATUS_DISCONNECTED"] = "ตัดการเชื่อมต่อ";
    m_translations[Language::TH]["ABOUT"] = "เกี่ยวกับ";
    m_translations[Language::TH]["DEVELOPER"] = "ผู้พัฒนา";
    m_translations[Language::TH]["EMAIL"] = "อีเมล";
    m_translations[Language::TH]["LANGUAGE"] = "ภาษา";
    m_translations[Language::TH]["YES"] = "ใช่";
    m_translations[Language::TH]["NO"] = "ไม่";
    m_translations[Language::TH]["START_SERVER"] = "เริ่มเซิร์ฟเวอร์";
    m_translations[Language::TH]["STOP_SERVER"] = "หยุดเซิร์ฟเวอร์";
    m_translations[Language::TH]["SERVER_CONTROL_PANEL"] = "แผงควบคุมเซิร์ฟเวอร์";
    m_translations[Language::TH]["CLIENT_CONNECTION"] = "การเชื่อมต่อลูกค้า";
    m_translations[Language::TH]["CLIENT_HINT"] = "กำหนดค่าพร็อกซีระบบหรือเบราว์เซอร์ของคุณเป็น SOCKS5 127.0.0.1:";

    // Mexican Spanish
    m_translations[Language::ES_MX] = std::unordered_map<std::string, std::string>();
    m_translations[Language::ES_MX]["APP_TITLE_SERVER"] = "Htkis-LANProxySerCli - Servidor";
    m_translations[Language::ES_MX]["APP_TITLE_CLIENT"] = "Htkis-LANProxySerCli - Cliente";
    m_translations[Language::ES_MX]["USER_MANAGEMENT"] = "Gestión de usuarios";
    m_translations[Language::ES_MX]["USERNAME"] = "Usuario";
    m_translations[Language::ES_MX]["PASSWORD"] = "Contraseña";
    m_translations[Language::ES_MX]["DAYS"] = "Días";
    m_translations[Language::ES_MX]["ADD_USER"] = "Agregar usuario";
    m_translations[Language::ES_MX]["REFRESH_LIST"] = "Actualizar lista";
    m_translations[Language::ES_MX]["ACTIVE"] = "Activo";
    m_translations[Language::ES_MX]["EXPIRE_TIME"] = "Fecha de vencimiento";
    m_translations[Language::ES_MX]["ACTIONS"] = "Acciones";
    m_translations[Language::ES_MX]["DEACTIVATE"] = "Desactivar";
    m_translations[Language::ES_MX]["ACTIVATE"] = "Activar";
    m_translations[Language::ES_MX]["DELETE"] = "Eliminar";
    m_translations[Language::ES_MX]["SERVER_IP"] = "IP del servidor";
    m_translations[Language::ES_MX]["SERVER_PORT"] = "Puerto del servidor";
    m_translations[Language::ES_MX]["LOCAL_PORT"] = "Puerto local";
    m_translations[Language::ES_MX]["CONNECT"] = "Conectar";
    m_translations[Language::ES_MX]["DISCONNECT"] = "Desconectar";
    m_translations[Language::ES_MX]["STATUS_RUNNING"] = "EJECUTANDO";
    m_translations[Language::ES_MX]["STATUS_STOPPED"] = "DETENIDO";
    m_translations[Language::ES_MX]["STATUS_CONNECTED"] = "CONECTADO";
    m_translations[Language::ES_MX]["STATUS_DISCONNECTED"] = "DESCONECTADO";
    m_translations[Language::ES_MX]["ABOUT"] = "Acerca de";
    m_translations[Language::ES_MX]["DEVELOPER"] = "Desarrollador";
    m_translations[Language::ES_MX]["EMAIL"] = "Correo";
    m_translations[Language::ES_MX]["LANGUAGE"] = "Idioma";
    m_translations[Language::ES_MX]["YES"] = "Sí";
    m_translations[Language::ES_MX]["NO"] = "No";
    m_translations[Language::ES_MX]["START_SERVER"] = "Iniciar servidor";
    m_translations[Language::ES_MX]["STOP_SERVER"] = "Detener servidor";
    m_translations[Language::ES_MX]["SERVER_CONTROL_PANEL"] = "Panel de control";
    m_translations[Language::ES_MX]["CLIENT_CONNECTION"] = "Conexión del cliente";
    m_translations[Language::ES_MX]["CLIENT_HINT"] = "Configure su sistema o navegador proxy a SOCKS5 127.0.0.1:";
    
    // Hungarian
    m_translations[Language::HU] = std::unordered_map<std::string, std::string>();
    m_translations[Language::HU]["APP_TITLE_SERVER"] = "Htkis-LANProxySerCli - Szerver";
    m_translations[Language::HU]["APP_TITLE_CLIENT"] = "Htkis-LANProxySerCli - Ügyfél";
    m_translations[Language::HU]["USER_MANAGEMENT"] = "Felhasználó kezelés";
    m_translations[Language::HU]["USERNAME"] = "Felhasználónév";
    m_translations[Language::HU]["PASSWORD"] = "Jelszó";
    m_translations[Language::HU]["DAYS"] = "Napok";
    m_translations[Language::HU]["ADD_USER"] = "Felhasználó hozzáadása";
    m_translations[Language::HU]["REFRESH_LIST"] = "Lista frissítése";
    m_translations[Language::HU]["ACTIVE"] = "Aktív";
    m_translations[Language::HU]["EXPIRE_TIME"] = "Lejárat ideje";
    m_translations[Language::HU]["ACTIONS"] = "Műveletek";
    m_translations[Language::HU]["DEACTIVATE"] = "Deaktiválás";
    m_translations[Language::HU]["ACTIVATE"] = "Aktiválás";
    m_translations[Language::HU]["DELETE"] = "Törlés";
    m_translations[Language::HU]["SERVER_IP"] = "Szerver IP";
    m_translations[Language::HU]["SERVER_PORT"] = "Szerver Port";
    m_translations[Language::HU]["LOCAL_PORT"] = "Helyi Port";
    m_translations[Language::HU]["CONNECT"] = "Csatlakozás";
    m_translations[Language::HU]["DISCONNECT"] = "Kapcsolat bontása";
    m_translations[Language::HU]["STATUS_RUNNING"] = "FUT";
    m_translations[Language::HU]["STATUS_STOPPED"] = "LEÁLLÍTVA";
    m_translations[Language::HU]["STATUS_CONNECTED"] = "CSATLAKOZTATVA";
    m_translations[Language::HU]["STATUS_DISCONNECTED"] = "NINCS KAPCSOLAT";
    m_translations[Language::HU]["ABOUT"] = "Névjegy";
    m_translations[Language::HU]["DEVELOPER"] = "Fejlesztő";
    m_translations[Language::HU]["EMAIL"] = "Email";
    m_translations[Language::HU]["LANGUAGE"] = "Nyelv";
    m_translations[Language::HU]["YES"] = "Igen";
    m_translations[Language::HU]["NO"] = "Nem";
    m_translations[Language::HU]["START_SERVER"] = "Szerver indítása";
    m_translations[Language::HU]["STOP_SERVER"] = "Szerver leállítása";
    m_translations[Language::HU]["SERVER_CONTROL_PANEL"] = "Szerver vezérlőpult";
    m_translations[Language::HU]["CLIENT_CONNECTION"] = "Ügyfél kapcsolat";
    m_translations[Language::HU]["CLIENT_HINT"] = "Állítsa be a rendszer vagy böngésző proxy-t SOCKS5 127.0.0.1:";
}

void Localization::setLanguage(Language lang) {
    m_current_lang = lang;
}

Language Localization::getLanguage() const {
    return m_current_lang;
}

std::string Localization::get(const std::string& key) {
    if (m_translations.count(m_current_lang) && m_translations[m_current_lang].count(key)) {
        return m_translations[m_current_lang][key];
    }
    // Fallback to English
    if (m_translations.count(Language::EN) && m_translations[Language::EN].count(key)) {
        return m_translations[Language::EN][key];
    }
    return key;
}

const std::vector<Localization::LangInfo>& Localization::getAvailableLanguages() const {
    return m_languages;
}
