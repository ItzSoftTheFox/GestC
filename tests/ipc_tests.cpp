#include <QCoreApplication>
#include <QTemporaryDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QLocalSocket>
#include <QElapsedTimer>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <stdexcept>
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc != 2) return 2;
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("QT_QPA_PLATFORM", "offscreen");
    environment.insert("QT_QUICK_BACKEND", "software");
    environment.insert("XDG_CONFIG_HOME", directory.path());
    environment.insert("XDG_RUNTIME_DIR", directory.path());
    QProcess process; process.setProcessEnvironment(environment); process.start(argv[1]);
    const auto cleanup = [&] { process.terminate(); if (!process.waitForFinished(3000)) { process.kill(); process.waitForFinished(); } };
    try {
        if (!process.waitForStarted()) throw std::runtime_error("application did not start");
        QElapsedTimer timer; timer.start(); bool ready = false;
        while (timer.elapsed() < 10000) {
            QLocalSocket socket; socket.connectToServer(directory.path() + "/handmouse.sock");
            if (socket.waitForConnected(100)) { ready = true; break; }
            if (process.state() == QProcess::NotRunning) break;
            QThread::msleep(50);
        }
        if (!ready) throw std::runtime_error("IPC socket did not become ready");
        const auto command = [&](const QString& option) {
            QProcess client; client.setProcessEnvironment(environment); client.start(argv[1], {option});
            if (!client.waitForFinished(5000) || client.exitCode() != 0) throw std::runtime_error("IPC command failed");
            return client.readAllStandardOutput();
        };
        auto state = QJsonDocument::fromJson(command("--status")).object();
        if (state["running"].toBool() || state["inputReady"].toBool()) throw std::runtime_error("startup unexpectedly enables hardware");
        command("--toggle-pause");
        state = QJsonDocument::fromJson(command("--status")).object();
        if (!state["paused"].toBool()) throw std::runtime_error("global pause command was not applied");
        command("--toggle-pause");
        state = QJsonDocument::fromJson(command("--status")).object();
        if (state["paused"].toBool()) throw std::runtime_error("global resume command was not applied");
        command("--stop"); cleanup();
        std::cout << "Single instance, status, global pause/resume and stop passed.\n";
        return 0;
    } catch (const std::exception& e) {
        cleanup(); std::cerr << e.what() << '\n' << process.readAllStandardError().constData(); return 1;
    }
}
