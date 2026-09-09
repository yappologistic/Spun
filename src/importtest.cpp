#include "player.h"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QDataStream>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <sys/stat.h>
#include <iostream>

static constexpr quint32 dataBytes = 32 * 1024 * 1024;
static bool makeWave(const QString &path) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QDataStream stream(&file); stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("RIFF", 4); stream << quint32(dataBytes + 36);
    stream.writeRawData("WAVEfmt ", 8); stream << quint32(16) << quint16(1) << quint16(1)
        << quint32(44100) << quint32(88200) << quint16(2) << quint16(16);
    stream.writeRawData("data", 4); stream << dataBytes;
    return stream.status() == QDataStream::Ok && file.resize(dataBytes + 44);
}
static bool waitImport(Player &player) {
    QElapsedTimer clock; clock.start();
    while (player.busy() && clock.elapsed() < 120000) QTest::qWait(5);
    return !player.busy();
}
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QTemporaryDir temp;
    if (!temp.isValid()) return 1;
    const int files = app.arguments().contains("--stress") ? 20000 : 250;
    const auto music = temp.path() + "/Music";
    QString firstFile;
    quint64 physicalBytes = 0;
    for (int i = 0; i < files; ++i) {
        const auto path = music + QString("/Artist %1/Album %2/%3.%4")
            .arg(i / 1000, 3, 10, QChar('0')).arg(i / 100, 3, 10, QChar('0'))
            .arg(i, 5, 10, QChar('0')).arg(i % 2 ? "WAV" : "wav");
        if (!makeWave(path)) return 1;
        if (i == 0) firstFile = path;
        struct stat info{};
        if (::stat(QFile::encodeName(path).constData(), &info) == 0) physicalBytes += quint64(info.st_blocks) * 512;
    }
    int failures = 0, checks = 0;
    auto check = [&](bool passed, const char *name) {
        ++checks;
        std::cout << (passed ? "PASS " : "FAIL ") << name << std::endl;
        if (!passed) ++failures;
    };
    const auto settings = temp.path() + "/player.ini";
    {
        Player player(settings);
        QSignalSpy progress(&player, &Player::importProgressChanged);
        QSignalSpy queueChanges(&player, &Player::queueChanged);
        QTimer heartbeat;
        QElapsedTimer clock; clock.start();
        qint64 lastBeat = 0, maxGap = 0;
        int beats = 0;
        QObject::connect(&heartbeat, &QTimer::timeout, [&] {
            const auto now = clock.elapsed(); maxGap = qMax(maxGap, now - lastBeat); lastBeat = now; ++beats;
        });
        heartbeat.start(5);
        player.addUrls({QUrl::fromLocalFile(music)}, false);
        player.addUrls({QUrl("https://example.invalid/song.mp3")}, false);
        check(player.busy() && player.error().isEmpty(), "repeated import request preserves active progress without a stale error");
        check(waitImport(player), "recursive import completes");
        maxGap = qMax(maxGap, clock.elapsed() - lastBeat);
        heartbeat.stop();
        std::cout << "IMPORT tracks=" << player.count() << " expected=" << files
                  << " elapsed_ms=" << clock.elapsed() << " logical_bytes=" << quint64(files) * (dataBytes + 44)
                  << " allocated_audio_bytes=" << physicalBytes << " heartbeat_count=" << beats
                  << " maximum_event_loop_gap_ms=" << maxGap << std::endl;
        check(player.count() == files, "all nested songs imported including uppercase extensions");
        check(player.error().isEmpty(), "successful import has no error");
        check(player.currentUrl().toLocalFile() == firstFile, "deterministic folder ordering");
        check(player.duration() > 300000, "valid sparse WAV metadata read");
        check(!player.playing(), "import does not autoplay when disabled");
        check(progress.count() >= 2 && player.importStatus().startsWith("Added "), "progress and completion status emitted");
        check(queueChanges.count() == 1, "queue published once per import");
        player.select(5, false);
        const auto selection = player.currentUrl();
        queueChanges.clear();
        check(QFile::link(firstFile, music + "/alias.wav"), "create file symlink fixture");
        check(QFile::link(music, music + "/cycle"), "create directory cycle fixture");
        const auto outside = temp.path() + "/Outside/new.wav";
        check(makeWave(outside) && QFile::link(QFileInfo(outside).absolutePath(), music + "/outside"), "create external directory link fixture");
        player.addUrls({QUrl::fromLocalFile(music), QUrl::fromLocalFile(QFileInfo(firstFile).absolutePath()), QUrl::fromLocalFile(firstFile)}, false);
        check(waitImport(player) && player.count() == files, "overlaps and file aliases deduplicated; directory symlinks skipped");
        check(queueChanges.isEmpty(), "duplicate-only import does not rebuild queue");
        check(player.currentUrl() == selection && player.error().isEmpty(), "reimport preserves selection and succeeds");
        player.addUrls({QUrl::fromLocalFile(outside)}, false);
        player.cancelImport();
        check(waitImport(player) && player.count() == files && player.importStatus() == "Import cancelled", "cancel discards pending additions");
        check(player.currentUrl() == selection, "cancel preserves current track");
        player.addUrls({QUrl("https://example.invalid/song.mp3"), QUrl::fromLocalFile(temp.path() + "/missing.wav")}, false);
        check(waitImport(player) && !player.error().isEmpty() && player.count() == files, "invalid inputs report error without clearing queue");
        player.addUrls({QUrl::fromLocalFile(outside)}, false);
        check(waitImport(player) && player.count() == files + 1 && player.error().isEmpty(), "direct-file import recovers after cancellation and error");
        check(player.currentUrl() == selection, "adding songs preserves current track");
    }
    {
        Player restored(settings);
        check(restored.count() == files + 1, "large queue persists and restores");
        restored.addUrls({QUrl::fromLocalFile(music)}, false);
        restored.clear();
        check(waitImport(restored) && restored.count() == 0, "clear cancels pending import without repopulating queue");
    }
    {
        Player interrupted(temp.path() + "/interrupted.ini");
        interrupted.addUrls({QUrl::fromLocalFile(music)}, false);
    }
    check(true, "destroying player during import finishes safely");
    std::cout << "RESULT " << failures << " failures / " << checks << " checks" << std::endl;
    return failures ? 1 : 0;
}
