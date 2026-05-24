// SPDX-License-Identifier: GPL-3.0-only

#include "AutyzmBootstrap.h"

#include "Application.h"
#include "BuildConfig.h"
#include "InstanceList.h"
#include "settings/INISettingsObject.h"
#include "ui/themes/ThemeManager.h"

#include <FileSystem.h>

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QTextStream>
#include <QUrl>

namespace {
constexpr auto kInstanceId = "pullappMC";
constexpr auto kInstanceName = "pullappMC";
constexpr auto kMinecraftVersion = "1.21.1";
constexpr auto kNeoForgeVersion = "21.1.229";
constexpr auto kServerName = "pullappMC";
constexpr auto kServerAddress = "minecraft.pullapp.xyz";

// Packwiz pack URL - mods are synced automatically on each launch
constexpr auto kPackwizPackUrl = "https://minecraft.pullapp.xyz/pack/pack.toml";

// Packwiz installer bootstrap - small JAR that downloads and runs the actual installer
constexpr auto kPackwizBootstrapUrl = "https://github.com/packwiz/packwiz-installer-bootstrap/releases/download/v0.0.3/packwiz-installer-bootstrap.jar";

bool writeTextFile(const QString& path, const QString& content)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Autyzm bootstrap: cannot write" << path << file.errorString();
        return false;
    }

    QTextStream out(&file);
    out << content;
    if (!file.commit()) {
        qWarning() << "Autyzm bootstrap: cannot commit" << path << file.errorString();
        return false;
    }
    return true;
}

bool writeTextFileIfMissing(const QString& path, const QString& content)
{
    if (QFileInfo::exists(path)) {
        return true;
    }
    return writeTextFile(path, content);
}

bool writeBinaryFileIfMissing(const QString& path, const QByteArray& content)
{
    if (QFileInfo::exists(path)) {
        return true;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Autyzm bootstrap: cannot write" << path << file.errorString();
        return false;
    }
    file.write(content);
    if (!file.commit()) {
        qWarning() << "Autyzm bootstrap: cannot commit" << path << file.errorString();
        return false;
    }
    return true;
}

void writeShort(QByteArray& out, quint16 value)
{
    out.append(char((value >> 8) & 0xff));
    out.append(char(value & 0xff));
}

void writeInt(QByteArray& out, quint32 value)
{
    out.append(char((value >> 24) & 0xff));
    out.append(char((value >> 16) & 0xff));
    out.append(char((value >> 8) & 0xff));
    out.append(char(value & 0xff));
}

void writeUtf(QByteArray& out, const QString& value)
{
    const QByteArray utf = value.toUtf8();
    writeShort(out, static_cast<quint16>(utf.size()));
    out.append(utf);
}

QByteArray makeServersDat()
{
    // Minimal uncompressed NBT for Minecraft's servers.dat:
    // TAG_Compound("") { TAG_List("servers", TAG_Compound, 1) [
    //   TAG_Compound("") { TAG_String("name"), TAG_String("ip"), TAG_End }
    // ], TAG_End }
    QByteArray nbt;
    nbt.append(char(10));      // TAG_Compound
    writeUtf(nbt, QString());  // root name
    nbt.append(char(9));       // TAG_List
    writeUtf(nbt, QStringLiteral("servers"));
    nbt.append(char(10));  // list element type: TAG_Compound
    writeInt(nbt, 1);      // one server
    nbt.append(char(8));   // TAG_String
    writeUtf(nbt, QStringLiteral("name"));
    writeUtf(nbt, QString::fromLatin1(kServerName));
    nbt.append(char(8));  // TAG_String
    writeUtf(nbt, QStringLiteral("ip"));
    writeUtf(nbt, QString::fromLatin1(kServerAddress));
    nbt.append(char(0));  // TAG_End for server compound
    nbt.append(char(0));  // TAG_End for root compound
    return nbt;
}

void downloadPackwizBootstrap(const QString& minecraftDir)
{
    const QString jarPath = FS::PathCombine(minecraftDir, "packwiz-installer-bootstrap.jar");
    
    if (QFileInfo::exists(jarPath)) {
        qInfo() << "Autyzm bootstrap: packwiz-installer-bootstrap.jar already exists";
        return;
    }

    if (!APPLICATION->network()) {
        qWarning() << "Autyzm bootstrap: network manager is not ready; cannot download packwiz bootstrap";
        return;
    }

    qInfo() << "Autyzm bootstrap: downloading packwiz-installer-bootstrap.jar";
    auto* reply = APPLICATION->network()->get(QNetworkRequest(QUrl(QString::fromLatin1(kPackwizBootstrapUrl))));
    QObject::connect(reply, &QNetworkReply::finished, [reply, jarPath]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Autyzm bootstrap: packwiz bootstrap download failed" << reply->errorString();
            return;
        }
        
        const QByteArray data = reply->readAll();
        
        QSaveFile out(jarPath);
        if (!out.open(QIODevice::WriteOnly)) {
            qWarning() << "Autyzm bootstrap: cannot save packwiz bootstrap" << jarPath << out.errorString();
            return;
        }
        out.write(data);
        if (!out.commit()) {
            qWarning() << "Autyzm bootstrap: cannot commit packwiz bootstrap" << jarPath << out.errorString();
            return;
        }
        qInfo() << "Autyzm bootstrap: packwiz-installer-bootstrap.jar downloaded successfully";
    });
}

}  // namespace

namespace AutyzmBootstrap {

void applyFirstRunDefaults()
{
    auto settings = APPLICATION->settings();

    // No first-run Java dialog. Prefer launcher-managed Java, especially for MC 1.21.x.
    settings->set("AutomaticJavaDownload", true);
    settings->set("AutomaticJavaSwitch", true);
    settings->set("UserAskedAboutAutomaticJavaDownload", true);
    settings->set("IgnoreJavaWizard", true);

    // Never block startup with CurseForge's unofficial API-key prompt.
    settings->set("FlameKeyShouldBeFetchedOnStartup", false);

    // Old-school compact theme requested for the branded launcher.
    if (settings->get("ApplicationTheme").toString().isEmpty() ||
        !APPLICATION->themeManager()->isValidApplicationTheme(settings->get("ApplicationTheme").toString())) {
        settings->set("ApplicationTheme", QStringLiteral("windows"));
    }
    if (settings->get("IconTheme").toString().isEmpty() ||
        !APPLICATION->themeManager()->isValidIconTheme(settings->get("IconTheme").toString())) {
        settings->set("IconTheme", QStringLiteral("pe_colored"));
    }
}

void ensureDefaultInstance()
{
    auto settings = APPLICATION->settings();
    const QString instDir = settings->get("InstanceDir").toString();
    if (instDir.isEmpty()) {
        qWarning() << "Autyzm bootstrap: InstanceDir is empty";
        return;
    }

    const QString instanceRoot = FS::PathCombine(instDir, QString::fromLatin1(kInstanceId));
    const QString minecraftDir = FS::PathCombine(instanceRoot, ".minecraft");
    if (!FS::ensureFolderPathExists(instanceRoot) || !FS::ensureFolderPathExists(minecraftDir)) {
        qWarning() << "Autyzm bootstrap: cannot create instance directories" << instanceRoot;
        return;
    }

    // Create mods folder for packwiz
    FS::ensureFolderPathExists(FS::PathCombine(minecraftDir, "mods"));

    const bool existed = QFileInfo::exists(FS::PathCombine(instanceRoot, "instance.cfg"));
    if (!existed) {
        qInfo() << "Autyzm bootstrap: creating default instance at" << instanceRoot;
    }

    // Pre-launch command runs packwiz-installer-bootstrap which syncs mods from the server.
    // -g = no GUI, -s client = client-side only mods, URL = pack.toml location
    // $INST_JAVA is expanded by the launcher to the Java executable path.
    // Note: Don't use quotes around $INST_JAVA - the INI parser handles paths with spaces correctly.
    const QString preLaunchCommand = QStringLiteral("$INST_JAVA -jar packwiz-installer-bootstrap.jar -g -s client %1")
                                         .arg(QString::fromLatin1(kPackwizPackUrl));

    // Always write instance.cfg to ensure PreLaunchCommand is set (even on existing instances)
    writeTextFile(FS::PathCombine(instanceRoot, "instance.cfg"),
                  QStringLiteral("ConfigVersion=1.3\n"
                                 "InstanceType=OneSix\n"
                                 "name=%1\n"
                                 "iconKey=grass\n"
                                 "ManagedPack=false\n"
                                 "OverrideCommands=true\n"
                                 "PreLaunchCommand=%2\n"
                                 "OverrideJava=true\n"
                                 "OverrideMemory=true\n"
                                 "MinMemAlloc=1024\n"
                                 "MaxMemAlloc=8192\n")
                      .arg(QString::fromLatin1(kInstanceName), preLaunchCommand));

    writeTextFileIfMissing(FS::PathCombine(instanceRoot, "mmc-pack.json"),
                           QStringLiteral("{\n"
                                          "    \"components\": [\n"
                                          "        {\n"
                                          "            \"cachedName\": \"Minecraft\",\n"
                                          "            \"cachedRequires\": [\n"
                                          "                {\n"
                                          "                    \"suggests\": \"21\",\n"
                                          "                    \"uid\": \"net.minecraft.java\"\n"
                                          "                }\n"
                                          "            ],\n"
                                          "            \"important\": true,\n"
                                          "            \"uid\": \"net.minecraft\",\n"
                                          "            \"version\": \"%1\"\n"
                                          "        },\n"
                                          "        {\n"
                                          "            \"cachedName\": \"NeoForge\",\n"
                                          "            \"cachedRequires\": [\n"
                                          "                {\n"
                                          "                    \"equals\": \"%1\",\n"
                                          "                    \"uid\": \"net.minecraft\"\n"
                                          "                }\n"
                                          "            ],\n"
                                          "            \"uid\": \"net.neoforged\",\n"
                                          "            \"version\": \"%2\"\n"
                                          "        }\n"
                                          "    ],\n"
                                          "    \"formatVersion\": 1\n"
                                          "}\n")
                               .arg(QString::fromLatin1(kMinecraftVersion), QString::fromLatin1(kNeoForgeVersion)));

    writeBinaryFileIfMissing(FS::PathCombine(minecraftDir, "servers.dat"), makeServersDat());

    writeTextFileIfMissing(FS::PathCombine(instanceRoot, "README-AUTYZM.txt"),
                           QStringLiteral("Autyzm.pl modpack instance.\n"
                                          "Minecraft: %1\n"
                                          "NeoForge: %2\n"
                                          "Server: %3\n"
                                          "\n"
                                          "Mods are automatically synced from:\n"
                                          "%4\n"
                                          "\n"
                                          "On each launch, packwiz-installer checks for mod updates\n"
                                          "and downloads any new or changed mods automatically.\n")
                               .arg(QString::fromLatin1(kMinecraftVersion), QString::fromLatin1(kNeoForgeVersion),
                                    QString::fromLatin1(kServerAddress), QString::fromLatin1(kPackwizPackUrl)));

    if (APPLICATION->instances()) {
        APPLICATION->instances()->loadList();
        APPLICATION->instances()->setInstanceGroup(QString::fromLatin1(kInstanceId), QStringLiteral("Autyzm.pl"));
    }

    // Download packwiz-installer-bootstrap.jar to .minecraft/
    downloadPackwizBootstrap(minecraftDir);
}

}  // namespace AutyzmBootstrap
