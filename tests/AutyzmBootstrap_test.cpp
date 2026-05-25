#include <QFile>
#include <QProcess>
#include <QTemporaryFile>
#include <QTest>
#include <QTextStream>

#include <AutyzmBootstrap.h>
#include <settings/INIFile.h>

class AutyzmBootstrapTest : public QObject {
    Q_OBJECT

   private slots:
    void test_RawUnescapedIniQuotesLosePreLaunchSeparator()
    {
        QTemporaryFile file;
        QVERIFY(file.open());
        const QString fileName = file.fileName();

        QTextStream stream(&file);
        stream << "ConfigVersion=1.3\n"
               << "PreLaunchCommand=\"${INST_JAVA}\" -jar packwiz-installer-bootstrap.jar\n";
        file.close();

        INIFile config;
        QVERIFY(config.loadFile(fileName));
        QCOMPARE(config.get(QStringLiteral("PreLaunchCommand"), QString()).toString(),
                 QStringLiteral("${INST_JAVA}-jar packwiz-installer-bootstrap.jar"));
    }

    void test_DefaultInstanceConfigPreservesQuotedJavaPath()
    {
        QTemporaryFile file;
        QVERIFY(file.open());
        const QString fileName = file.fileName();
        file.close();

        QVERIFY(AutyzmBootstrap::writeDefaultInstanceConfig(fileName));

        QFile rawFile(fileName);
        QVERIFY(rawFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString raw = QString::fromUtf8(rawFile.readAll());
        QVERIFY2(raw.contains(QStringLiteral("PreLaunchCommand=\\\"${INST_JAVA}\\\" -jar ")), qPrintable(raw));
        QVERIFY2(!raw.contains(QStringLiteral("PreLaunchCommand=\"${INST_JAVA}\" -jar ")), qPrintable(raw));

        INIFile config;
        QVERIFY(config.loadFile(fileName));
        const QString command = config.get(QStringLiteral("PreLaunchCommand"), QString()).toString();
        QCOMPARE(command, AutyzmBootstrap::defaultPreLaunchCommand());

        QString expanded = command;
        const QString javaPath =
            QStringLiteral("/Users/franek/Library/Application Support/AutyzmLauncher/java/java-runtime-delta/bin/java");
        expanded.replace(QStringLiteral("${INST_JAVA}"), javaPath);

        const auto args = QProcess::splitCommand(expanded);
        QCOMPARE(args.size(), 7);
        QCOMPARE(args.at(0), javaPath);
        QCOMPARE(args.at(1), QStringLiteral("-jar"));
        QCOMPARE(args.at(2), QStringLiteral("packwiz-installer-bootstrap.jar"));
    }
};

QTEST_GUILESS_MAIN(AutyzmBootstrapTest)

#include "AutyzmBootstrap_test.moc"
