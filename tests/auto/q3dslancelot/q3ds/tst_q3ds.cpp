/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of Qt 3D Studio.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qbaselinetest.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QDirIterator>
#include <QtCore/QDebug>
#include <QtCore/QProcess>
#include <QtCore/QStandardPaths>
#include <QtGui/QImage>

#include <algorithm>


QString blockify(const QByteArray& s)
{
    const char* indent = "\n | ";
    QByteArray block = s.trimmed();
    block.replace('\n', indent);
    block.prepend(indent);
    block.append('\n');
    return block;
}


class tst_Q3DS : public QObject
{
    Q_OBJECT
public:
    tst_Q3DS();

private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void testRendering_data();
    void testRendering();

private:
    void setupTestSuite(const QByteArray& filter = QByteArray());
    void runTest(const QStringList& extraArgs = QStringList());
    bool renderAndGrab(const QString& qmlFile, const QStringList& extraArgs, QImage *screenshot, QString *errMsg);
    quint16 checksumFileOrDir(const QString &path);
    int consecutiveErrors;   // Not test failures (image mismatches), but system failures (so no image at all)
    bool aborted = false;            // This run given up because of too many system failures
};

tst_Q3DS::tst_Q3DS()
{

}

void tst_Q3DS::initTestCase()
{
    QByteArray msg;
    if (!QBaselineTest::connectToBaselineServer(&msg))
        QSKIP(msg);
}

void tst_Q3DS::cleanup()
{
    // Allow subsystems time to settle
    if (!aborted)
        QTest::qWait(200);
}

void tst_Q3DS::testRendering_data()
{
    setupTestSuite();
    consecutiveErrors = 0;
    aborted = false;
}

void tst_Q3DS::testRendering()
{
    runTest();
}

void tst_Q3DS::setupTestSuite(const QByteArray &filter)
{
    QTest::addColumn<QString>("uipFile");
    int numItems = 0;

    QString testSuiteDir = QLatin1String("data");
    QString testSuiteLocation = QCoreApplication::applicationDirPath();
    QString testSuitePath = testSuiteLocation + QDir::separator() + testSuiteDir;
    QFileInfo fi(testSuitePath);
    if (!fi.exists() || !fi.isDir() || !fi.isReadable())
        QSKIP("Test suite data directory missing or unreadable: " + testSuitePath.toLatin1());

    QStringList ignoreItems;
    QFile ignoreFile(testSuitePath + "/Ignore");
    if (ignoreFile.open(QIODevice::ReadOnly)) {
        while (!ignoreFile.atEnd()) {
            QByteArray line = ignoreFile.readLine().trimmed();
            if (!line.isEmpty() && !line.startsWith('#'))
                ignoreItems += line;
        }
    }

    QStringList itemFiles;
    QDirIterator it(testSuitePath, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString fp = it.next();
        if (fp.endsWith(".uip") || fp.endsWith(".uia")) {
            QString itemName = fp.mid(testSuitePath.length() + 1);
            if (!ignoreItems.contains(itemName) && (filter.isEmpty() || !itemName.startsWith(filter)))
                itemFiles.append(it.filePath());
        }
    }

    std::sort(itemFiles.begin(), itemFiles.end());
    for (const QString &filePath : qAsConst(itemFiles)) {
        QByteArray itemName = filePath.mid(testSuitePath.length() + 1).toLatin1();
        QBaselineTest::newRow(itemName, checksumFileOrDir(filePath)) << filePath;
        numItems++;
    }

    if (!numItems)
        QSKIP("No Qt 3D Studio test files found in " + testSuitePath.toLatin1());
}

void tst_Q3DS::runTest(const QStringList &extraArgs)
{
    if (aborted)
        QSKIP("System too unstable.");

    QFETCH(QString, uipFile);

    QImage screenShot;
    QString errorMessage;
    if (renderAndGrab(uipFile, extraArgs, &screenShot, &errorMessage)) {
        consecutiveErrors = 0;
    }
    else {
        if (++consecutiveErrors >= 3)
            aborted = true; // Just give up if screen grabbing fails 3 times in a row
        QFAIL(qPrintable("Qt 3D Studio grabbing failed: " + errorMessage));
    }

    QBASELINE_TEST(screenShot);
}

bool tst_Q3DS::renderAndGrab(const QString &uipFile, const QStringList &extraArgs, QImage *screenshot, QString *errMsg)
{
    bool usePipe = true;  // Whether to transport the grabbed image using temp. file or pipe. TBD: cmdline option
    bool q3ds1 = qEnvironmentVariableIsSet("QT_LANCELOT_Q3DS1");
    QString cmd;
    if (!q3ds1)
        cmd = QCoreApplication::applicationDirPath() + "/q3dsscenegrabber";
    else
        cmd = QCoreApplication::applicationDirPath() + "/q3ds1scenegrabber";
#ifdef Q_OS_WIN
    usePipe = false;
    if (!q3ds1)
        cmd = QCoreApplication::applicationDirPath() + "/q3dsscenegrabber.exe";
    else
        cmd = QCoreApplication::applicationDirPath() + "/q3ds1scenegrabber.exe";
#endif
    QProcess grabber;
    QStringList args = extraArgs;
    QString tmpfile = usePipe ? QString("-") : QString("%1/q3dsscenegrabber-%2-out.ppm").arg(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).arg(QCoreApplication::applicationPid());
    args << uipFile << "-o" << tmpfile;
    grabber.start(cmd, args, QIODevice::ReadOnly);
    grabber.waitForFinished(17000);         //### hardcoded, must be larger than the scene timeout in qmlscenegrabber
    if (grabber.state() != QProcess::NotRunning) {
        grabber.terminate();
        grabber.waitForFinished(3000);
    }
    QImage img;
    bool res = usePipe ? img.load(&grabber, "ppm") : img.load(tmpfile);
    if (!res || img.isNull()) {
        if (errMsg) {
            QString s("Failed to grab screen. qmlscenegrabber exitcode: %1. Process error: %2. Stderr:%3");
            *errMsg = s.arg(grabber.exitCode()).arg(grabber.errorString()).arg(blockify(grabber.readAllStandardError()));
        }
        if (!usePipe)
            QFile::remove(tmpfile);
        return false;
    }
    if (screenshot)
        *screenshot = img;
    if (!usePipe)
        QFile::remove(tmpfile);
    return true;
}

quint16 tst_Q3DS::checksumFileOrDir(const QString &path)
{
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isReadable())
        return 0;
    if (fi.isFile()) {
        QFile f(path);
        f.open(QIODevice::ReadOnly);
        QByteArray contents = f.readAll();
        return qChecksum(contents.constData(), contents.size());
    }
    if (fi.isDir()) {
        static const QStringList nameFilters = QStringList() << "*.qml" << "*.cpp" << "*.png" << "*.jpg";
        quint16 cs = 0;
        const auto entryList = QDir(fi.filePath()).entryList(nameFilters,
                                                             QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        for (const QString &item : entryList)
            cs ^= checksumFileOrDir(path + QLatin1Char('/') + item);
        return cs;
    }
    return 0;
}

#define main _realmain
QTEST_MAIN(tst_Q3DS)
#undef main

int main(int argc, char *argv[])
{
    QBaselineTest::handleCmdLineArgs(&argc, &argv);
    return _realmain(argc, argv);
}

#include "tst_q3ds.moc"
