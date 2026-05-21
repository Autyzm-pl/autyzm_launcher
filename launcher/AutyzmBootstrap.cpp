// SPDX-License-Identifier: GPL-3.0-only

#include "AutyzmBootstrap.h"

#include "Application.h"
#include "BuildConfig.h"
#include "InstanceList.h"
#include "settings/INISettingsObject.h"
#include "ui/themes/ThemeManager.h"

#include <FileSystem.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

namespace {
constexpr auto kInstanceId = "Autyzm";
constexpr auto kInstanceName = "Autyzm.pl";
constexpr auto kMinecraftVersion = "1.21.1";
constexpr auto kServerAddress = "minecraft.pullapp.xyz";

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
    if (QFileInfo::exists(FS::PathCombine(instanceRoot, "instance.cfg"))) {
        return;
    }

    qInfo() << "Autyzm bootstrap: creating default instance at" << instanceRoot;
    if (!FS::ensureFolderPathExists(instanceRoot) || !FS::ensureFolderPathExists(FS::PathCombine(instanceRoot, ".minecraft"))) {
        qWarning() << "Autyzm bootstrap: cannot create instance directories" << instanceRoot;
        return;
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
                       "        }\n"
                       "    ],\n"
                       "    \"formatVersion\": 1\n"
                       "}\n")
            .arg(QString::fromLatin1(kMinecraftVersion)));

    // Human-readable marker for now. The real modpack bootstrap/update source can replace this
    // with packwiz/.mrpack import once the canonical modpack URL is ready.
    writeTextFileIfMissing(
        FS::PathCombine(instanceRoot, "README-AUTYZM.txt"),
        QStringLiteral("Autyzm.pl default instance placeholder.\n"
                       "Minecraft: %1\n"
                       "Server: %2\n"
                       "TODO: attach packwiz/mrpack bootstrap source and pre-seed server list.\n")
            .arg(QString::fromLatin1(kMinecraftVersion), QString::fromLatin1(kServerAddress)));

    if (APPLICATION->instances()) {
        APPLICATION->instances()->loadList();
        APPLICATION->instances()->setInstanceGroup(QString::fromLatin1(kInstanceId), QStringLiteral("Autyzm.pl"));
    }
}

}
