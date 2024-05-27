// SPDX-FileCopyrightText: 2015 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <map>
#include <string>
#include <utility>

#include <QCoreApplication>
#include <QFileInfo>
#include <QObject>
#include <QStandardItem>
#include <QString>
#include <QWidget>

#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "sudachi/play_time_manager.h"
#include "sudachi/uisettings.h"
#include "sudachi/util/util.h"

enum class GameListItemType {
    Game = QStandardItem::UserType + 1,
    CustomDir = QStandardItem::UserType + 2,
    SdmcDir = QStandardItem::UserType + 3,
    UserNandDir = QStandardItem::UserType + 4,
    SysNandDir = QStandardItem::UserType + 5,
    AddDir = QStandardItem::UserType + 6,
    Favorites = QStandardItem::UserType + 7,
};

Q_DECLARE_METATYPE(GameListItemType);

/**
 * Gets the default icon (for games without valid title metadata)
 * @param size The desired width and height of the default icon.
 * @return QPixmap default icon
 */
static QPixmap GetDefaultIcon(u32 size) {
    QPixmap icon(size, size);
    icon.fill(Qt::transparent);
    return icon;
}

class GameListItem : public QStandardItem {

public:
    // used to access type from item index
    static constexpr int TypeRole = Qt::UserRole + 1;
    static constexpr int SortRole = Qt::UserRole + 2;
    GameListItem() = default;
    explicit GameListItem(const QString& string) : QStandardItem(string) {
        setData(string, SortRole);
    }
};

/**
 * A specialization of GameListItem for path values.
 * This class ensures that for every full path value it holds, a correct string representation
 * of just the filename (with no extension) will be displayed to the user.
 * If this class receives valid title metadata, it will also display game icons and titles.
 */
class GameListItemPath : public GameListItem {
public:
    static constexpr int TitleRole = SortRole + 1;
    static constexpr int FullPathRole = SortRole + 2;
    static constexpr int ProgramIdRole = SortRole + 3;
    static constexpr int FileTypeRole = SortRole + 4;

    GameListItemPath() = default;
    GameListItemPath(const QString& game_path, const std::vector<u8>& picture_data,
                     const QString& game_name, const QString& game_type, u64 program_id) {
        setData(type(), TypeRole);
        setData(game_path, FullPathRole);
        setData(game_name, TitleRole);
        setData(qulonglong(program_id), ProgramIdRole);
        setData(game_type, FileTypeRole);

        const u32 size = UISettings::values.game_icon_size.GetValue();

        QPixmap picture;
        if (!picture.loadFromData(picture_data.data(), static_cast<u32>(picture_data.size()))) {
            picture = GetDefaultIcon(size);
        }
        picture = picture.scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        setData(picture, Qt::DecorationRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Game);
    }

    QVariant data(int role) const override {
        if (role == Qt::DisplayRole || role == SortRole) {
            std::string filename;
            Common::SplitPath(data(FullPathRole).toString().toStdString(), nullptr, &filename,
                              nullptr);

            const std::array<QString, 4> row_data{{
                QString::fromStdString(filename),
                data(FileTypeRole).toString(),
                QString::fromStdString(fmt::format("0x{:016X}", data(ProgramIdRole).toULongLong())),
                data(TitleRole).toString(),
            }};

            const auto& row1 = row_data.at(UISettings::values.row_1_text_id.GetValue());
            const int row2_id = UISettings::values.row_2_text_id.GetValue();

            if (role == SortRole) {
                return row1.toLower();
            }

            // None
            if (row2_id == 4) {
                return row1;
            }

            const auto& row2 = row_data.at(row2_id);

            if (row1 == row2) {
                return row1;
            }

            return QStringLiteral("%1\n    %2").arg(row1, row2);
        }

        return GameListItem::data(role);
    }
};

class GameListItemCompat : public GameListItem {
    Q_DECLARE_TR_FUNCTIONS(GameListItemCompat)
public:
    static constexpr int CompatNumberRole = SortRole;
    GameListItemCompat() = default;
    explicit GameListItemCompat(const QString& compatibility) {
        setData(type(), TypeRole);

        struct CompatStatus {
            QString color;
            const char* text;
            const char* tooltip;
        };
        // clang-format off
        const auto ingame_status =
                       CompatStatus{QStringLiteral("#f2d624"), QT_TR_NOOP("Ingame"),     QT_TR_NOOP("Game starts, but crashes or major glitches prevent it from being completed.")};
        static const std::map<QString, CompatStatus> status_data = {
            {QStringLiteral("0"),  {QStringLiteral("#5c93ed"), QT_TR_NOOP("Perfect"),    QT_TR_NOOP("Game can be played without issues.")}},
            {QStringLiteral("1"),  {QStringLiteral("#47d35c"), QT_TR_NOOP("Playable"),   QT_TR_NOOP("Game functions with minor graphical or audio glitches and is playable from start to finish.")}},
            {QStringLiteral("2"),  ingame_status},
            {QStringLiteral("3"),  ingame_status}, // Fallback for the removed "Okay" category
            {QStringLiteral("4"),  {QStringLiteral("#FF0000"), QT_TR_NOOP("Intro/Menu"), QT_TR_NOOP("Game loads, but is unable to progress past the Start Screen.")}},
            {QStringLiteral("5"),  {QStringLiteral("#828282"), QT_TR_NOOP("Won't Boot"), QT_TR_NOOP("The game crashes when attempting to startup.")}},
            {QStringLiteral("99"), {QStringLiteral("#000000"), QT_TR_NOOP("Not Tested"), QT_TR_NOOP("The game has not yet been tested.")}},
        };
        // clang-format on

        auto iterator = status_data.find(compatibility);
        if (iterator == status_data.end()) {
            LOG_WARNING(Frontend, "Invalid compatibility number {}", compatibility.toStdString());
            return;
        }
        const CompatStatus& status = iterator->second;
        setData(compatibility, CompatNumberRole);
        setText(tr(status.text));
        setToolTip(tr(status.tooltip));
        setData(CreateCirclePixmapFromColor(status.color), Qt::DecorationRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Game);
    }

    bool operator<(const QStandardItem& other) const override {
        return data(CompatNumberRole).value<QString>() <
               other.data(CompatNumberRole).value<QString>();
    }
};

/**
 * A specialization of GameListItem for size values.
 * This class ensures that for every numerical size value it holds (in bytes), a correct
 * human-readable string representation will be displayed to the user.
 */
class GameListItemSize : public GameListItem {
public:
    static constexpr int SizeRole = SortRole;

    GameListItemSize() = default;
    explicit GameListItemSize(const qulonglong size_bytes) {
        setData(type(), TypeRole);
        setData(size_bytes, SizeRole);
    }

    void setData(const QVariant& value, int role) override {
        // By specializing setData for SizeRole, we can ensure that the numerical and string
        // representations of the data are always accurate and in the correct format.
        if (role == SizeRole) {
            qulonglong size_bytes = value.toULongLong();
            GameListItem::setData(ReadableByteSize(size_bytes), Qt::DisplayRole);
            GameListItem::setData(value, SizeRole);
        } else {
            GameListItem::setData(value, role);
        }
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Game);
    }

    /**
     * This operator is, in practice, only used by the TreeView sorting systems.
     * Override it so that it will correctly sort by numerical value instead of by string
     * representation.
     */
    bool operator<(const QStandardItem& other) const override {
        return data(SizeRole).toULongLong() < other.data(SizeRole).toULongLong();
    }
};

/**
 * GameListItem for Play Time values.
 * This object stores the play time of a game in seconds, and its readable
 * representation in minutes/hours
 */
class GameListItemPlayTime : public GameListItem {
public:
    static constexpr int PlayTimeRole = SortRole;

    GameListItemPlayTime() = default;
    explicit GameListItemPlayTime(const qulonglong time_seconds) {
        setData(time_seconds, PlayTimeRole);
    }

    void setData(const QVariant& value, int role) override {
        qulonglong time_seconds = value.toULongLong();
        GameListItem::setData(PlayTime::ReadablePlayTime(time_seconds), Qt::DisplayRole);
        GameListItem::setData(value, PlayTimeRole);
    }

    bool operator<(const QStandardItem& other) const override {
        return data(PlayTimeRole).toULongLong() < other.data(PlayTimeRole).toULongLong();
    }
};

/**
 * GameListItem for Play Time values.
 * This object stores the play time of a game in seconds, and its readable
 * representation in minutes/hours
 */
class GameListItemTotalTimes : public GameListItem {
public:
    static constexpr int TotalTimesRole = SortRole;

    GameListItemTotalTimes() = default;
    explicit GameListItemTotalTimes(const qulonglong times_count) {
        setData(times_count, TotalTimesRole);
    }

    void setData(const QVariant& value, int role) override {
        qulonglong times_count = value.toULongLong();
        GameListItem::setData(PlayTime::ReadableTotalTimes(times_count), Qt::DisplayRole);
        GameListItem::setData(value, TotalTimesRole);
    }

    bool operator<(const QStandardItem& other) const override {
        return data(TotalTimesRole).toULongLong() < other.data(TotalTimesRole).toULongLong();
    }
};

class GameListDir : public GameListItem {
public:
    static constexpr int GameDirRole = Qt::UserRole + 2;

    explicit GameListDir(UISettings::GameDir& directory,
                         GameListItemType dir_type_ = GameListItemType::CustomDir)
        : dir_type{dir_type_} {
        setData(type(), TypeRole);

        UISettings::GameDir* game_dir = &directory;
        setData(QVariant(UISettings::values.game_dirs.indexOf(directory)), GameDirRole);

        const int icon_size = UISettings::values.folder_icon_size.GetValue();
        switch (dir_type) {
        case GameListItemType::SdmcDir:
            setData(
                QIcon::fromTheme(QStringLiteral("sd_card"))
                    .pixmap(icon_size)
                    .scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
            setData(QObject::tr("Installed SD Titles"), Qt::DisplayRole);
            break;
        case GameListItemType::UserNandDir:
            setData(
                QIcon::fromTheme(QStringLiteral("chip"))
                    .pixmap(icon_size)
                    .scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
            setData(QObject::tr("Installed NAND Titles"), Qt::DisplayRole);
            break;
        case GameListItemType::SysNandDir:
            setData(
                QIcon::fromTheme(QStringLiteral("chip"))
                    .pixmap(icon_size)
                    .scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
            setData(QObject::tr("System Titles"), Qt::DisplayRole);
            break;
        case GameListItemType::CustomDir: {
            const QString path = QString::fromStdString(game_dir->path);
            const QString icon_name =
                QFileInfo::exists(path) ? QStringLiteral("folder") : QStringLiteral("bad_folder");
            setData(QIcon::fromTheme(icon_name).pixmap(icon_size).scaled(
                        icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                    Qt::DecorationRole);
            setData(path, Qt::DisplayRole);
            break;
        }
        default:
            break;
        }
    }

    int type() const override {
        return static_cast<int>(dir_type);
    }

    /**
     * Override to prevent automatic sorting between folders and the addDir button.
     */
    bool operator<(const QStandardItem& other) const override {
        return false;
    }

private:
    GameListItemType dir_type;
};

class GameListAddDir : public GameListItem {
public:
    explicit GameListAddDir() {
        setData(type(), TypeRole);

        const int icon_size = UISettings::values.folder_icon_size.GetValue();

        setData(QIcon::fromTheme(QStringLiteral("list-add"))
                    .pixmap(icon_size)
                    .scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
        setData(QObject::tr("Add New Game Directory"), Qt::DisplayRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::AddDir);
    }

    bool operator<(const QStandardItem& other) const override {
        return false;
    }
};

class GameListFavorites : public GameListItem {
public:
    explicit GameListFavorites() {
        setData(type(), TypeRole);

        const int icon_size = UISettings::values.folder_icon_size.GetValue();

        setData(QIcon::fromTheme(QStringLiteral("star"))
                    .pixmap(icon_size)
                    .scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
        setData(QObject::tr("Favorites"), Qt::DisplayRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Favorites);
    }

    bool operator<(const QStandardItem& other) const override {
        return false;
    }
};

class GameList;
class QHBoxLayout;
class QTreeView;
class QLabel;
class QLineEdit;
class QToolButton;

class GameListSearchField : public QWidget {
    Q_OBJECT

public:
    explicit GameListSearchField(GameList* parent = nullptr);

    QString filterText() const;
    void setFilterResult(int visible_, int total_);

    void clear();
    void setFocus();

private:
    void changeEvent(QEvent*) override;
    void RetranslateUI();

    class KeyReleaseEater : public QObject {
    public:
        explicit KeyReleaseEater(GameList* gamelist_, QObject* parent = nullptr);

    private:
        GameList* gamelist = nullptr;
        QString edit_filter_text_old;

    protected:
        // EventFilter in order to process systemkeys while editing the searchfield
        bool eventFilter(QObject* obj, QEvent* event) override;
    };
    int visible;
    int total;

    QHBoxLayout* layout_filter = nullptr;
    QTreeView* tree_view = nullptr;
    QLabel* label_filter = nullptr;
    QLineEdit* edit_filter = nullptr;
    QLabel* label_filter_result = nullptr;
    QToolButton* button_filter_close = nullptr;
};
