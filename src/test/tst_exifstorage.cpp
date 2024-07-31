#include <QSharedPointer>
#include <QSignalSpy>
#include <QTestEventLoop>

#include <gtest/gtest.h>

#include "exifstorage.h"
#include "tmpjpegfile.h"
#include "tst.h"

class ExifStorageTest : public QTestEventLoop
{
    QString mWithoutExif = TmpJpegFile::withoutExif();
    QString mWithGps = TmpJpegFile::withGps();
    QString mWithoutGps = TmpJpegFile::withoutGps();

    QStringList mFirstPass, mSecondPass;

    void ready(const QSharedPointer<Photo>& photo) {
        const QString& path = photo->path;
        if (photo->pix16.isNull()) {
            // no thumbnail found in EXIF, file added to the second processing queue
            ExifStorage::cancel(path); // remove it from all processing queues
            mFirstPass.append(path);
            // QThread::msleep(50);
            ExifStorage::data(path); // and now we're going to request the data again
        } else {
            if (mFirstPass.contains(path)) {
                // despite canceling the thumbnail was still read
                mSecondPass.append(path);

                if (mSecondPass.size() == mFirstPass.size()) // great, job done!
                    exitLoop();
            }
        }
    }

public:
    ExifStorageTest() {
        connect(ExifStorage::instance(), &ExifStorage::ready, this, &ExifStorageTest::ready);
    }

   ~ExifStorageTest() {
        ExifStorage::destroy();
        std::sort(mFirstPass.begin(), mFirstPass.end());
        std::sort(mSecondPass.begin(), mSecondPass.end());
        EXPECT_FALSE(mFirstPass.isEmpty());
        EXPECT_EQ(mFirstPass, mSecondPass);
    }

   void start() {
       ExifStorage::parse(mWithoutExif);
       ExifStorage::parse(mWithGps);
       ExifStorage::parse(mWithoutGps);
       enterLoopMSecs(3000);
   }
};

TEST(ExifStorage, canceling)
{
    ExifStorageTest test;
    test.start();
}
