// Test that QProcess::splitCommand correctly handles quoted paths with spaces
// This verifies the fix for macOS "Application Support" paths in PreLaunchCommand

#include <QProcess>
#include <QTest>

class PreLaunchCommandTest : public QObject {
    Q_OBJECT
private slots:
    void test_splitCommand_unquoted_path_with_spaces()
    {
        // WITHOUT quotes: path with space gets split incorrectly
        QString cmd = "/Users/franek/Library/Application Support/AutyzmLauncher/java/bin/java -jar packwiz.jar";
        auto args = QProcess::splitCommand(cmd);
        
        // This is WRONG - first arg is truncated at the space
        QCOMPARE(args.at(0), "/Users/franek/Library/Application");
        QCOMPARE(args.at(1), "Support/AutyzmLauncher/java/bin/java");
        // The rest is garbage from here
    }

    void test_splitCommand_quoted_path_with_spaces()
    {
        // WITH quotes: path with space is preserved as single argument
        QString cmd = "\"/Users/franek/Library/Application Support/AutyzmLauncher/java/bin/java\" -jar packwiz.jar";
        auto args = QProcess::splitCommand(cmd);
        
        // This is CORRECT - full path preserved
        QCOMPARE(args.at(0), "/Users/franek/Library/Application Support/AutyzmLauncher/java/bin/java");
        QCOMPARE(args.at(1), "-jar");
        QCOMPARE(args.at(2), "packwiz.jar");
        QCOMPARE(args.size(), 3);
    }

    void test_splitCommand_our_prelaunch_format()
    {
        // Test the exact format we use after variable expansion
        QString javaPath = "/Users/franek/Library/Application Support/AutyzmLauncher/java/java-runtime-delta/bin/java";
        QString cmd = QString("\"%1\" -jar packwiz-installer-bootstrap.jar -g -s client https://minecraft.pullapp.xyz/pack/pack.toml").arg(javaPath);
        
        auto args = QProcess::splitCommand(cmd);
        
        QCOMPARE(args.size(), 7);
        QCOMPARE(args.at(0), javaPath);  // Full path with space preserved
        QCOMPARE(args.at(1), "-jar");
        QCOMPARE(args.at(2), "packwiz-installer-bootstrap.jar");
        QCOMPARE(args.at(3), "-g");
        QCOMPARE(args.at(4), "-s");
        QCOMPARE(args.at(5), "client");
        QCOMPARE(args.at(6), "https://minecraft.pullapp.xyz/pack/pack.toml");
    }
};

QTEST_GUILESS_MAIN(PreLaunchCommandTest)
#include "PreLaunchCommand_test.moc"
