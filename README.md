# تم و پلاگین مدرن تیم‌اسپیک ۳ (TeamSpeak 3 Modern Black Suite)
طراحی‌شده توسط استودیو **Coretify Studio**

---

## 📦 پکیج همه‌کاره و یکپارچه (All-In-One Addon)

تمامی اجزای تم، پلاگین تایتل‌بار ویندوز ۱۱، سیستم تغییر رنگ زنده و موتور آپدیت خودکار در یک پکیج واحد ادغام شده‌اند:

### 📥 **[`ModernBlack.ts3_addon`](file:///g:/Projects/TS3-ModernBlack/ModernBlack.ts3_addon)**
تنها با یک دابل‌کلیک روی این فایل، تمام بخش‌ها به طور خودکار در تیم‌اسپیک ۳ نصب و فعال می‌شوند:
1. **تم مشکی مدرن (Obsidian Dark Theme):** رنگ پایه `#060709`، خط گرادیانی زیر منوبار، اسکرول‌بار کپسولی باریک و طراحی اختصاصی چت و پنل‌ها.
2. **تایتل‌بار مشکی ویندوز ۱۱ (Native Dark TitleBar):** هماهنگ‌سازی نوار عنوان پنجره‌ها با رنگ تیره سیستم‌عامل با فریم اختصاصی.
3. **کاستومایزر زنده رنگ و اکسنت (Theme Customizer):** انتخاب فوری پالت‌های رنگی و تغییر آنی تم بدون نیاز به ریستارت.
4. **سیستم بررسی و آپدیت خودکار (Auto-Updater):** چک کردن نسخه سرور در پس‌زمینه و امکان آپدیت با یک کلیک.

---

## ⚙️ تنظیمات نسخه و آپدیت خودکار (`.env`)

تنظیمات نسخه و سرور آپدیت به سادگی از طریق فایل [`.env`](file:///g:/Projects/TS3-ModernBlack/.env) کنترل می‌شوند:

```env
# Suite & Plugin Version (فرمت SemVer)
PLUGIN_VERSION=1.0.0

# آدرس فایل JSON متادیتای آپدیت (روی گیتهاب یا سرور اختصاصی)
UPDATE_URL=https://raw.githubusercontent.com/Coretify/TS3-ModernBlack/main/version.json
```

### ساختار فایل `version.json` روی سرور / گیت‌هاب:
```json
{
  "version": "1.0.0",
  "download_url": "https://github.com/Coretify/TS3-ModernBlack/releases/download/v1.0.0/ModernBlack.ts3_addon",
  "changelog": "Unified Modern Black Suite: Sleek Obsidian Dark Theme, Windows 11 Dark Titlebars, Live Accent Customizer, and Auto-Update support in 1 package."
}
```

---

## 🛠️ نحوه بیلد و کامپایل خودکار

برای کامپایل مجدد پلاگین و ایجاد بسته نصبی `ModernBlack.ts3_addon`، یکی از دستورات زیر را اجرا کنید:

1. **اجرای مستقیم اسکریپت پایتون:**
   ```bash
   python build.py
   ```
2. **یا دابل‌کلیک روی فایل [`build.bat`](file:///g:/Projects/TS3-ModernBlack/build.bat)**.

---

## 📁 ساختار منظم پروژه:
```
├── .env                                # تنظیمات نسخه و لینک سرور آپدیت
├── .env.example                        # فایل نمونه کانفیگ
├── version.json                        # الگوی متادیتای آنلاین سرور
├── package.ini                         # مانیفست اددان تیم‌اسپیک ۳
├── build.py                            # سیستم یکپارچه بیلد، کامپایل و پکیجینگ
├── build.bat                           # کلید میانبر اجرای سریع بیلد در ویندوز
├── ModernBlack.ts3_addon               # بسته نهایی قابل نصب در TeamSpeak 3
├── styles/                             # فایل‌های استایل QSS و قالب‌های TPL
└── plugins/                            # سورس و فایل DLL پلاگین یکپارچه
    ├── modern_black.cpp                # سورس C++ (تایتل‌بار + کاستومایزر + آپدیتر)
    ├── modern_black_win64.dll          # فایل DLL نهایی کامپایل‌شده 64-بیتی
    ├── qss_template.h                  # قالب داخلی استایل تم
    ├── version_config.h                # هدر تولید شده از .env
    └── build_plugin.bat                # اسکریپت کامپایل تکی پلاگین
```
