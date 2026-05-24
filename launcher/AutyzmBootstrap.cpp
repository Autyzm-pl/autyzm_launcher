// SPDX-License-Identifier: GPL-3.0-only

#include "AutyzmBootstrap.h"

#include "Application.h"
#include "BuildConfig.h"
#include "InstanceList.h"
#include "MMCZip.h"
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
constexpr auto kInstanceId = "Autyzm";
constexpr auto kInstanceName = "Autyzm.pl";
constexpr auto kMinecraftVersion = "1.21.1";
constexpr auto kNeoForgeVersion = "21.1.229";
constexpr auto kServerName = "Autyzm.pl";
constexpr auto kServerAddress = "minecraft.pullapp.xyz";
constexpr auto kClientPackUrl =
    "https://github.com/Autyzm-pl/autyzm_launcher/releases/download/v0.1.0-alpha.5/autyzm-client-pack.zip";
constexpr auto kClientPackSha256 = "806237b55a1917c1df8697f0e2f589ab801d62faab01d65ff43ddbaadb1cfde4";

bool writeTextFileIfMissing(const QString& path, const QString& content)
{
    if (QFileInfo::exists(path)) {
        return true;
    }

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
    nbt.append(char(10));      // list element type: TAG_Compound
    writeInt(nbt, 1);          // one server
    nbt.append(char(8));       // TAG_String
    writeUtf(nbt, QStringLiteral("name"));
    writeUtf(nbt, QString::fromLatin1(kServerName));
    nbt.append(char(8));       // TAG_String
    writeUtf(nbt, QStringLiteral("ip"));
    writeUtf(nbt, QString::fromLatin1(kServerAddress));
    nbt.append(char(0));       // TAG_End for server compound
    nbt.append(char(0));       // TAG_End for root compound
    return nbt;
}

QString clientPackMarkerPath(const QString& instanceRoot)
{
    return FS::PathCombine(instanceRoot, QStringLiteral(".autyzm-client-pack-%1.txt").arg(QString::fromLatin1(kClientPackSha256)));
}

void installClientPackFromZip(const QString& instanceRoot, const QString& zipPath)
{
    const QString marker = clientPackMarkerPath(instanceRoot);
    if (QFileInfo::exists(marker)) {
        qInfo() << "Autyzm bootstrap: client pack already installed";
        return;
    }

    const QByteArray expectedSha = QByteArray(kClientPackSha256);
    QFile zip(zipPath);
    if (!zip.open(QIODevice::ReadOnly)) {
        qWarning() << "Autyzm bootstrap: cannot open client pack" << zipPath << zip.errorString();
        return;
    }
    const QByteArray actualSha = QCryptographicHash::hash(zip.readAll(), QCryptographicHash::Sha256).toHex();
    if (actualSha != expectedSha) {
        qWarning() << "Autyzm bootstrap: client pack checksum mismatch" << actualSha << "expected" << expectedSha;
        return;
    }

    const QString minecraftDir = FS::PathCombine(instanceRoot, ".minecraft");
    qInfo() << "Autyzm bootstrap: extracting client pack" << zipPath << "to" << minecraftDir;
    const auto extracted = MMCZip::extractDir(zipPath, minecraftDir);
    if (!extracted.has_value()) {
        qWarning() << "Autyzm bootstrap: failed to extract client pack" << zipPath;
        return;
    }

    writeTextFileIfMissing(marker, QStringLiteral("sha256=%1\nurl=%2\n").arg(QString::fromLatin1(kClientPackSha256), QString::fromLatin1(kClientPackUrl)));
    if (APPLICATION->instances()) {
        APPLICATION->instances()->loadList();
    }
}

void downloadAndInstallClientPack(const QString& instanceRoot)
{
    if (!APPLICATION->network()) {
        qWarning() << "Autyzm bootstrap: network manager is not ready; cannot download client pack";
        return;
    }

    const QString cacheDir = FS::PathCombine(APPLICATION->root(), "cache");
    FS::ensureFolderPathExists(cacheDir);
    const QString zipPath = FS::PathCombine(cacheDir, QStringLiteral("autyzm-client-pack-%1.zip").arg(QString::fromLatin1(kClientPackSha256)));

    if (QFileInfo::exists(zipPath)) {
        installClientPackFromZip(instanceRoot, zipPath);
        return;
    }

    qInfo() << "Autyzm bootstrap: downloading client pack" << kClientPackUrl;
    auto* reply = APPLICATION->network()->get(QNetworkRequest(QUrl(QString::fromLatin1(kClientPackUrl))));
    QObject::connect(reply, &QNetworkReply::finished, [reply, zipPath, instanceRoot]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Autyzm bootstrap: client pack download failed" << reply->errorString();
            return;
        }
        QSaveFile out(zipPath);
        if (!out.open(QIODevice::WriteOnly)) {
            qWarning() << "Autyzm bootstrap: cannot save client pack" << zipPath << out.errorString();
            return;
        }
        out.write(reply->readAll());
        if (!out.commit()) {
            qWarning() << "Autyzm bootstrap: cannot commit client pack" << zipPath << out.errorString();
            return;
        }
        installClientPackFromZip(instanceRoot, zipPath);
    });
}
}

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

    const bool existed = QFileInfo::exists(FS::PathCombine(instanceRoot, "instance.cfg"));
    if (!existed) {
        qInfo() << "Autyzm bootstrap: creating default instance at" << instanceRoot;
    }

    if (!writeTextFileIfMissing(
            FS::PathCombine(instanceRoot, "instance.cfg"),
            QStringLiteral("ConfigVersion=1.3\n"
                           "InstanceType=OneSix\n"
                           "name=%1\n"
                           "iconKey=grass\n"
                           "ManagedPack=false\n"
                           "OverrideJava=true\n"
                           "OverrideMemory=true\n"
                           "MinMemAlloc=1024\n"
                           "MaxMemAlloc=8192\n")
                .arg(QString::fromLatin1(kInstanceName)))) {
        return;
    }

    writeTextFileIfMissing(
        FS::PathCombine(instanceRoot, "mmc-pack.json"),
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

    writeTextFileIfMissing(
        FS::PathCombine(instanceRoot, "README-AUTYZM.txt"),
        QStringLiteral("Autyzm.pl default modpack instance.\n"
                       "Minecraft: %1\n"
                       "NeoForge: %2\n"
                       "Server: %3\n"
                       "Client pack: %4\n")
            .arg(QString::fromLatin1(kMinecraftVersion), QString::fromLatin1(kNeoForgeVersion), QString::fromLatin1(kServerAddress),
                 QString::fromLatin1(kClientPackUrl)));

    if (APPLICATION->instances()) {
        APPLICATION->instances()->loadList();
        APPLICATION->instances()->setInstanceGroup(QString::fromLatin1(kInstanceId), QStringLiteral("Autyzm.pl"));
    }

    downloadAndInstallClientPack(instanceRoot);
}

}
