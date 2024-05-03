#include <QCoreApplication>
#include <QDirIterator>
#include <QDebug>
#include <QFileInfo>
#include <QStandardPaths>

#include <QExifImageHeader>

#include <gtest/gtest.h>

#include "exif/utils.h"
#include "tmpjpegfile.h"


void PrintTo(const QPair<quint32, quint32>& value, std::ostream* str)
{
    *str << "{ " << value.first << "; " << value.second << " }";
}


TEST(QExifImageHeader, withoutExif)
{
    QString jpeg = TmpJpegFile::withoutExif();
    ASSERT_FALSE(jpeg.isEmpty()) << TmpJpegFile::lastError();

    double lat = 58.7203335774538746;
    QByteArray lat_ref = "N";

    auto generated = qDMS(lat);

    ASSERT_EQ(58u, generated[0].first);
    ASSERT_EQ(1u,  generated[0].second);
    ASSERT_EQ(43u, generated[1].first);
    ASSERT_EQ(1u, generated[1].second);
    ASSERT_EQ(132009u, generated[2].first);
    ASSERT_EQ(10000u, generated[2].second);

    {
        // save
        QExifImageHeader header;
        header.setValue(QExifImageHeader::GpsLatitude, generated);
        header.setValue(QExifImageHeader::GpsLatitudeRef, lat_ref);
        ASSERT_TRUE(header.saveToJpeg(jpeg));
    }

    {
        // load
        QExifImageHeader header;
        ASSERT_TRUE(header.loadFromJpeg(jpeg));

        EXPECT_EQ(generated, header.value(QExifImageHeader::GpsLatitude).toRationalVector());
        EXPECT_EQ(lat_ref, header.value(QExifImageHeader::GpsLatitudeRef).toByteArray());
    }
}

TEST(QExifImageHeader, withoutGps)
{
    QString jpeg = TmpJpegFile::withoutGps();
    ASSERT_FALSE(jpeg.isEmpty()) << TmpJpegFile::lastError();

    double lat = 42.0;
    QByteArray lat_ref = "W";
    QByteArray lon_ref = "S";

    auto generated = qDMS(lat);

    ASSERT_EQ(42u, generated[0].first);
    ASSERT_EQ(1u,  generated[0].second);
    ASSERT_EQ(0u, generated[1].first);
    ASSERT_EQ(1u, generated[1].second);
    ASSERT_EQ(0u, generated[2].first);
    ASSERT_EQ(10000u, generated[2].second);

    {
        // save
        QExifImageHeader header;
        ASSERT_TRUE(header.loadFromJpeg(jpeg));
        header.setValue(QExifImageHeader::GpsLatitude, generated);
        header.setValue(QExifImageHeader::GpsLatitudeRef, lat_ref);
        header.setValue(QExifImageHeader::GpsLongitude, generated);
        header.setValue(QExifImageHeader::GpsLongitudeRef, lon_ref);
        ASSERT_TRUE(header.saveToJpeg(jpeg));
    }

    {
        // load
        QExifImageHeader header;
        ASSERT_TRUE(header.loadFromJpeg(jpeg));

        EXPECT_EQ(generated, header.value(QExifImageHeader::GpsLatitude).toRationalVector());
        EXPECT_EQ(generated, header.value(QExifImageHeader::GpsLongitude).toRationalVector());

        EXPECT_EQ(lat_ref, header.value(QExifImageHeader::GpsLatitudeRef).toByteArray());
        EXPECT_EQ(lat_ref, header.value(QExifImageHeader::GpsLongitudeRef).toByteArray());
    }
}

TEST(QExifImageHeader, withGps)
{
    QString jpeg = TmpJpegFile::withoutGps();
    ASSERT_FALSE(jpeg.isEmpty()) << TmpJpegFile::lastError();

    double lat = 42.0;
    QByteArray lat_ref = "W";
    QByteArray lon_ref = "S";

    auto generated = qDMS(lat);

    ASSERT_EQ(42u, generated[0].first);
    ASSERT_EQ(1u,  generated[0].second);
    ASSERT_EQ(0u, generated[1].first);
    ASSERT_EQ(1u, generated[1].second);
    ASSERT_EQ(0u, generated[2].first);
    ASSERT_EQ(10000u, generated[2].second);

    {
        // save
        QExifImageHeader header;
        ASSERT_TRUE(header.loadFromJpeg(jpeg));

        ASSERT_NE(generated, header.value(QExifImageHeader::GpsLatitude).toRationalVector());
        ASSERT_NE(generated, header.value(QExifImageHeader::GpsLongitude).toRationalVector());
        ASSERT_NE(lat_ref, header.value(QExifImageHeader::GpsLatitudeRef).toByteArray());
        ASSERT_NE(lon_ref, header.value(QExifImageHeader::GpsLongitudeRef).toByteArray());

        header.setValue(QExifImageHeader::GpsLatitude, generated);
        header.setValue(QExifImageHeader::GpsLatitudeRef, lat_ref);
        header.setValue(QExifImageHeader::GpsLongitude, generated);
        header.setValue(QExifImageHeader::GpsLongitudeRef, lon_ref);

        ASSERT_TRUE(header.saveToJpeg(jpeg));
    }

    {
        // load
        QExifImageHeader header;
        ASSERT_TRUE(header.loadFromJpeg(jpeg));

        auto loaded = header.value(QExifImageHeader::GpsLatitude).toRationalVector();
        ASSERT_EQ(3, loaded.size());
        EXPECT_EQ(generated, loaded);

        loaded = header.value(QExifImageHeader::GpsLongitude).toRationalVector();
        ASSERT_EQ(3, loaded.size());

        EXPECT_EQ(generated[0].first,   loaded[0].first);
        EXPECT_EQ(generated[0].second,  loaded[0].second);
        EXPECT_EQ(generated[1].first,   loaded[1].first);
        EXPECT_EQ(generated[1].second,  loaded[1].second);
        EXPECT_EQ(generated[2].first,   loaded[2].first);
        EXPECT_EQ(generated[2].second,  loaded[2].second);

        EXPECT_EQ(lat_ref, header.value(QExifImageHeader::GpsLatitudeRef).toByteArray());
        EXPECT_EQ(lat_ref, header.value(QExifImageHeader::GpsLongitudeRef).toByteArray());
    }
}

TEST(QExifImageHeader, datetime)
{
    const QPair<QExifImageHeader::ImageTag, QByteArray>
        tagDT1 = { QExifImageHeader::DateTime, "1983:04:09 23:10:00" };
    const QPair<QExifImageHeader::ExifExtendedTag, QByteArray>
        tagDT2 = { QExifImageHeader::DateTimeOriginal, "1983:04:09 23:15:00" }; // 'Date Taken' in FastStone
    const QPair<QExifImageHeader::ExifExtendedTag, QByteArray>
        tagDT3 = { QExifImageHeader::DateTimeDigitized, "1983:04:09 23:20:00" };

    QString jpeg = TmpJpegFile::withGps();
    ASSERT_FALSE(jpeg.isEmpty()) << TmpJpegFile::lastError();

    QVector<QExifURational> lat, lon;
    QByteArray latRef, lonRef;

    {
        QExifImageHeader header;

        ASSERT_TRUE(header.loadFromJpeg(jpeg));

        lat = header.value(QExifImageHeader::GpsLatitude).toRationalVector();
        lon = header.value(QExifImageHeader::GpsLongitude).toRationalVector();
        latRef = header.value(QExifImageHeader::GpsLatitudeRef).toByteArray();
        lonRef = header.value(QExifImageHeader::GpsLongitudeRef).toByteArray();

        QByteArray previous = header.value(tagDT1.first).toByteArray();
        ASSERT_FALSE(previous.isEmpty());
        ASSERT_NE(previous, tagDT1.second);
        header.setValue(tagDT1.first, tagDT1.second);

        previous = header.value(tagDT2.first).toByteArray();
        ASSERT_FALSE(previous.isEmpty());
        ASSERT_NE(previous, tagDT2.second);
        header.setValue(tagDT2.first, tagDT2.second);

        previous = header.value(tagDT3.first).toByteArray();
        ASSERT_FALSE(previous.isEmpty());
        ASSERT_NE(previous, tagDT3.second);
        header.setValue(tagDT3.first, tagDT3.second);

        ASSERT_TRUE(header.saveToJpeg(jpeg));
    }

    {
        QExifImageHeader header;
        ASSERT_TRUE(header.loadFromJpeg(jpeg));
        EXPECT_EQ(tagDT1.second, header.value(tagDT1.first).toByteArray());
        EXPECT_EQ(tagDT2.second, header.value(tagDT2.first).toByteArray());
        EXPECT_EQ(tagDT3.second, header.value(tagDT3.first).toByteArray());

        EXPECT_EQ(lat, header.value(QExifImageHeader::GpsLatitude).toRationalVector()) << "EXIF is broken!";
        EXPECT_EQ(lon, header.value(QExifImageHeader::GpsLongitude).toRationalVector()) << "EXIF is broken!";
        EXPECT_EQ(latRef, header.value(QExifImageHeader::GpsLatitudeRef).toByteArray()) << "EXIF is broken!";
        EXPECT_EQ(lonRef, header.value(QExifImageHeader::GpsLongitudeRef).toByteArray()) << "EXIF is broken!";
    }
}
