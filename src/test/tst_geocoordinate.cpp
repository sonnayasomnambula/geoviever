#include <gtest/gtest.h>

#include "geocoordinate.h"
#include "tst.h"

TEST(GeoCoordinate, split)
{
    {
        QString s = "aaa";
        QVector<QStringRef> parts = GeoCoordinate::split(QStringRef(&s), {'.'});
        ASSERT_EQ(1, parts.size());
        EXPECT_EQ("aaa", parts.front());
    }

    {
        QString s = "aaa.";
        QVector<QStringRef> parts = GeoCoordinate::split(QStringRef(&s), {'.'});
        ASSERT_EQ(1, parts.size());
        EXPECT_EQ("aaa", parts.front());
    }

    {
        QString s = ".aaa";
        QVector<QStringRef> parts = GeoCoordinate::split(QStringRef(&s), {'.'});
        ASSERT_EQ(1, parts.size());
        EXPECT_EQ("aaa", parts.front());
    }

    {
        QString s = ".aaa.";
        QVector<QStringRef> parts = GeoCoordinate::split(QStringRef(&s), {'.'});
        ASSERT_EQ(1, parts.size());
        EXPECT_EQ("aaa", parts.front());
    }

    {
        QString s = "aaa.bbb";
        QVector<QStringRef> parts = GeoCoordinate::split(QStringRef(&s), {'.'});
        ASSERT_EQ(2, parts.size());
        EXPECT_EQ("aaa", parts.front());
        EXPECT_EQ("bbb", parts.back());
    }

    {
        QString s = ".aaa..bbb.";
        QVector<QStringRef> parts = GeoCoordinate::split(QStringRef(&s), {'.'});
        ASSERT_EQ(2, parts.size());
        EXPECT_EQ("aaa", parts.front());
        EXPECT_EQ("bbb", parts.back());
    }
}

TEST(GeoCoordinate, CoordinateFormats)
{
    const double eps = 0.0001;

    QGeoCoordinate expected(-27.46758, 153.02789, 28.1);
    for (QGeoCoordinate::CoordinateFormat format: {
             QGeoCoordinate::Degrees,
             QGeoCoordinate::DegreesWithHemisphere,
             QGeoCoordinate::DegreesMinutes,
             QGeoCoordinate::DegreesMinutesWithHemisphere,
             QGeoCoordinate::DegreesMinutesSeconds,
             QGeoCoordinate::DegreesMinutesSecondsWithHemisphere
         })
    {
        const QString text = expected.toString(format);
        QGeoCoordinate actual = GeoCoordinate::fromString(text);
        EXPECT_TRUE(actual.isValid())
            << "failed with format = " << format;

        EXPECT_NEAR(expected.latitude(), actual.latitude(), eps)
            << "failed with format = " << format;

        EXPECT_NEAR(expected.longitude(), actual.longitude(), eps)
            << "failed with format = " << format;

        EXPECT_NEAR(expected.altitude(), actual.altitude(), eps)
            << "failed with format = " << format;
    }
}

TEST(GeoCoordinate, yandex)
{
    const double eps = 0.0001;

    const QString text = "-20.486359, 46.252603";
    QGeoCoordinate actual = GeoCoordinate::fromString(text);
    QGeoCoordinate expected(-20.486359, 46.252603);

    EXPECT_TRUE(actual.isValid());
    EXPECT_NEAR(expected.latitude(), actual.latitude(), eps);
    EXPECT_NEAR(expected.longitude(), actual.longitude(), eps);
}

TEST(GeoCoordinate, google)
{
    const double eps = 0.0001;

    const QString text = R"(59°56'18.1"N 30°15'22.2"E)";
    QGeoCoordinate actual = GeoCoordinate::fromString(text);
    QGeoCoordinate expected(59.93836, 30.25617);

    EXPECT_TRUE(actual.isValid());
    EXPECT_NEAR(expected.latitude(), actual.latitude(), eps);
    EXPECT_NEAR(expected.longitude(), actual.longitude(), eps);
}

TEST(GeoCoordinate, 2gis)
{
    const double eps = 0.0001;

    const QString text = "-19.11685° 47.187718°";
    QGeoCoordinate actual = GeoCoordinate::fromString(text);
    QGeoCoordinate expected(-19.11685, 47.187718);

    EXPECT_TRUE(actual.isValid());
    EXPECT_NEAR(expected.latitude(), actual.latitude(), eps);
    EXPECT_NEAR(expected.longitude(), actual.longitude(), eps);
}
