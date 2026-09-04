#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include "../src/filesystem/FileEnumerator.h"
#include "../src/filesystem/FileWriter.h"
#include "../src/filesystem/FileReader.h"
#include "../src/platform/PlatformFilesystem.h"

using namespace FastTransfer;

class TestFilesystem : public QObject {
    Q_OBJECT
private slots:
    void testFolderEnumeration() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QString gameDir = tempDir.filePath("Cyberpunk");
        QDir().mkpath(gameDir + "/data");
        QDir().mkpath(gameDir + "/mods");

        QFile f1(gameDir + "/game.exe");
        QVERIFY(f1.open(QIODevice::WriteOnly));
        f1.write(QByteArray(100, 'X'));
        f1.close();

        QFile f2(gameDir + "/data/a.bin");
        QVERIFY(f2.open(QIODevice::WriteOnly));
        f2.write(QByteArray(250, 'Y'));
        f2.close();

        QFile f3(gameDir + "/mods/mod.zip");
        QVERIFY(f3.open(QIODevice::WriteOnly));
        f3.write(QByteArray(50, 'Z'));
        f3.close();

        FileManifest manifest = FileEnumerator::enumeratePaths(QStringList() << gameDir);
        QCOMPARE(manifest.totalFiles, 3ULL);
        QCOMPARE(manifest.totalBytes, 400ULL);

        QStringList relPaths;
        for (const auto& it : manifest.items) {
            relPaths.append(it.relativePath);
        }

        QVERIFY(relPaths.contains("Cyberpunk/game.exe"));
        QVERIFY(relPaths.contains("Cyberpunk/data/a.bin"));
        QVERIFY(relPaths.contains("Cyberpunk/mods/mod.zip"));
    }

    void testFileWriterPartialCommit() {
        QTemporaryDir outDir;
        QVERIFY(outDir.isValid());

        QByteArray data = "ImportantBackupContent1234567890";
        QString expectedHash = Blake3Hasher::hashBytes(data);

        FileWriter writer;
        bool ok = writer.open(outDir.path(), "Gaming-PC", "Backups/data.bin", data.size(), 0, DuplicatePolicy::Rename);
        QVERIFY(ok);

        QVERIFY(QFile::exists(writer.partialFilePath()));
        QVERIFY(!QFile::exists(writer.targetFilePath()));

        QVERIFY(writer.writeChunk(0, data));
        QVERIFY(writer.finalizeAndCommit(expectedHash));

        // .partial should be gone and target file present
        QVERIFY(!QFile::exists(writer.partialFilePath()));
        QVERIFY(QFile::exists(writer.targetFilePath()));

        QFile writtenFile(writer.targetFilePath());
        QVERIFY(writtenFile.open(QIODevice::ReadOnly));
        QCOMPARE(writtenFile.readAll(), data);
    }

    void testFileWriterChecksumMismatchPreservesPartial() {
        QTemporaryDir outDir;
        QVERIFY(outDir.isValid());

        QByteArray data = "CorruptedBytesData";
        QString badExpectedHash = "0000000000000000000000000000000000000000000000000000000000000000";

        FileWriter writer;
        QVERIFY(writer.open(outDir.path(), "Gaming-PC", "file.bin", data.size(), 0));
        QVERIFY(writer.writeChunk(0, data));

        // Finalize with mismatched expected hash
        QVERIFY(!writer.finalizeAndCommit(badExpectedHash));

        // .partial MUST be preserved for resume/retry
        QVERIFY(QFile::exists(writer.partialFilePath()));
        QVERIFY(!QFile::exists(writer.targetFilePath()));
    }

    void testUniqueNameResolution() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QString target = tempDir.filePath("Game.zip");
        QFile f(target);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();

        QString unique = FileWriter::resolveUniqueName(target);
        QVERIFY(unique.endsWith("Game (1).zip"));
    }
};

QTEST_MAIN(TestFilesystem)
#include "test_filesystem.moc"
