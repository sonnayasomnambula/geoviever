#include <QCoreApplication>
#include <QDirIterator>
#include <QDebug>
#include <QFileInfo>
#include <QPixmap>
#include <QStandardPaths>

#include <gtest/gtest.h>

#include "exif/file.h"
#include "exif/utils.h"

#include "tmpjpegfile.h"

bool operator ==(const ExifRational& L, const ExifRational& R) {
    return L.numerator == R.numerator && L.denominator == R.denominator;
}

void PrintTo(const QVector<ExifRational>& val, ::std::ostream* os) {
    *os << "{ ";
    for (int i = 0; i < val.size(); ++i) {
        *os << val[i].numerator << "/" << val[i].denominator << (i == val.size() - 1 ? " }" : "; ");
    }
}

void PrintTo(const QString& str, ::std::ostream* os) {
    *os << "\"" << qPrintable(str) << "\"";
}

TEST(ExifFile, notOpened)
{
    Exif::File exif;

    exif.setValue(EXIF_IFD_GPS, Exif::Tag::GPS::LATITUDE, Exif::Utils::toDMS(42));
    exif.setValue(EXIF_IFD_GPS, Exif::Tag::GPS::LATITUDE_REF, QByteArray("test"));
    exif.setValue(EXIF_IFD_0, EXIF_TAG_XP_KEYWORDS, QString("test"));

    EXPECT_TRUE(exif.uRationalVector(EXIF_IFD_GPS, Exif::Tag::GPS::LONGITUDE).isEmpty());
    EXPECT_TRUE(exif.ascii(EXIF_IFD_GPS, Exif::Tag::GPS::LONGITUDE_REF).isEmpty());

    EXPECT_TRUE(exif.values(EXIF_IFD_0).isEmpty());
    EXPECT_TRUE(exif.value(EXIF_IFD_0, EXIF_TAG_EXIF_VERSION).isNull());

//    EXPECT_TRUE(exif.thumbnail().isNull()); // requre QGuiApplication

    EXPECT_EQ(Exif::Orientation::Unknown, exif.orientation());
    EXPECT_EQ(0, exif.width());
    EXPECT_EQ(0, exif.height());

    // check for null safety
    exif.setValue(EXIF_IFD_GPS, Exif::Tag::GPS::LATITUDE, Exif::Utils::toDMS(500));

    for (ExifIfd ifd: { EXIF_IFD_0, EXIF_IFD_1, EXIF_IFD_EXIF, EXIF_IFD_GPS })
    {
        exif.setValue(ifd, EXIF_TAG_CAMERA_OWNER_NAME, QByteArray("test"));
        exif.setValue(ifd, EXIF_TAG_USER_COMMENT, QString("test"));
    }
}

TEST(ExifFile, removeTag)
{
    QString jpeg = TmpJpegFile::withGps();
    ASSERT_FALSE(jpeg.isEmpty()) << TmpJpegFile::lastError();

    QMap<ExifIfd, ExifTag> tags = {
        { EXIF_IFD_0, EXIF_TAG_MODEL },
        { EXIF_IFD_1, EXIF_TAG_IMAGE_WIDTH },
        { EXIF_IFD_EXIF, EXIF_TAG_DATE_TIME_ORIGINAL },
        { EXIF_IFD_GPS, Exif::Tag::GPS::LATITUDE },
    };

    for (auto i = tags.cbegin(); i != tags.cend(); ++i)
    {
        ExifIfd ifd = i.key();
        ExifTag tag = i.value();

        {
            Exif::File exif;
            ASSERT_TRUE(exif.load(jpeg, false));
            EXPECT_FALSE(exif.value(ifd, tag).isNull());
            exif.remove(ifd, tag);
            ASSERT_TRUE(exif.save(jpeg));
        }

        {
            Exif::File exif;
            ASSERT_TRUE(exif.load(jpeg, false));
            EXPECT_TRUE(exif.value(ifd, tag).isNull());
        }
    }
}

TEST(ExifFile, readWrite)
{
    double lat = 58.7203335774538746;
    QByteArray lat_ref = "N";
    QString keywords = "ночь; улица; фонарь; аптека";
    QString comment = "КГ/АМ";

    auto generated = Exif::Utils::toDMS(lat);

    ASSERT_EQ(3, generated.size());

    ASSERT_EQ(58u, generated[0].numerator);
    ASSERT_EQ(1u,  generated[0].denominator);
    ASSERT_EQ(43u, generated[1].numerator);
    ASSERT_EQ(1u, generated[1].denominator);
    ASSERT_EQ(132009u, generated[2].numerator);
    ASSERT_EQ(10000u, generated[2].denominator);

    for (const QString& jpeg: { TmpJpegFile::withoutExif(), TmpJpegFile::withoutGps(), TmpJpegFile::withGps() })
    {
        ASSERT_FALSE(jpeg.isEmpty()) << TmpJpegFile::lastError();

        {
            // save
            Exif::File exif;
            ASSERT_TRUE(exif.load(jpeg));

            exif.setValue(EXIF_IFD_GPS, Exif::Tag::GPS::LATITUDE, generated);
            exif.setValue(EXIF_IFD_GPS, Exif::Tag::GPS::LATITUDE_REF, lat_ref);

            exif.setValue(EXIF_IFD_EXIF, EXIF_TAG_USER_COMMENT, comment);
            exif.setValue(EXIF_IFD_0, EXIF_TAG_XP_KEYWORDS, keywords);

            ASSERT_TRUE(exif.save(jpeg));
        }

        {
            // load
            Exif::File exif;
            ASSERT_TRUE(exif.load(jpeg, false));

            EXPECT_EQ(generated, exif.uRationalVector(EXIF_IFD_GPS, Exif::Tag::GPS::LATITUDE));
            EXPECT_EQ(lat_ref, exif.value(EXIF_IFD_GPS, Exif::Tag::GPS::LATITUDE_REF).toByteArray());

            EXPECT_EQ(keywords, exif.value(EXIF_IFD_0, EXIF_TAG_XP_KEYWORDS).toString());
            EXPECT_EQ(comment, exif.value(EXIF_IFD_EXIF, EXIF_TAG_USER_COMMENT).toString());
        }
    }
}
