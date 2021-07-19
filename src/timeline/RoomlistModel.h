// SPDX-FileCopyrightText: 2021 Nheko Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <CacheStructs.h>
#include <QAbstractListModel>
#include <QHash>
#include <QSharedPointer>
#include <QSortFilterProxyModel>
#include <QString>
#include <set>

#include <mtx/responses/sync.hpp>

#include "TimelineModel.h"

class TimelineViewManager;

class RoomPreview
{
        Q_GADGET
        Q_PROPERTY(QString roomid READ roomid CONSTANT)
        Q_PROPERTY(QString roomName READ roomName CONSTANT)
        Q_PROPERTY(QString roomTopic READ roomTopic CONSTANT)
        Q_PROPERTY(QString roomAvatarUrl READ roomAvatarUrl CONSTANT)
        Q_PROPERTY(bool isInvite READ isInvite CONSTANT)

public:
        RoomPreview() {}

        QString roomid() const { return roomid_; }
        QString roomName() const { return roomName_; }
        QString roomTopic() const { return roomTopic_; }
        QString roomAvatarUrl() const { return roomAvatarUrl_; }
        bool isInvite() const { return isInvite_; }

        QString roomid_, roomName_, roomAvatarUrl_, roomTopic_;
        bool isInvite_ = false;
};

class RoomlistModel : public QAbstractListModel
{
        Q_OBJECT
        Q_PROPERTY(TimelineModel *currentRoom READ currentRoom NOTIFY currentRoomChanged RESET
                     resetCurrentRoom)
        Q_PROPERTY(RoomPreview currentRoomPreview READ currentRoomPreview NOTIFY currentRoomChanged
                     RESET resetCurrentRoom)
public:
        enum Roles
        {
                AvatarUrl = Qt::UserRole,
                RoomName,
                RoomId,
                LastMessage,
                Time,
                Timestamp,
                HasUnreadMessages,
                HasLoudNotification,
                NotificationCount,
                IsInvite,
                IsSpace,
                IsPreview,
                IsPreviewFetched,
                Tags,
                ParentSpaces,
        };

        RoomlistModel(TimelineViewManager *parent = nullptr);
        QHash<int, QByteArray> roleNames() const override;
        int rowCount(const QModelIndex &parent = QModelIndex()) const override
        {
                (void)parent;
                return (int)roomids.size();
        }
        QVariant data(const QModelIndex &index, int role) const override;
        QSharedPointer<TimelineModel> getRoomById(QString id) const
        {
                if (models.contains(id))
                        return models.value(id);
                else
                        return {};
        }

public slots:
        void initializeRooms();
        void sync(const mtx::responses::Rooms &rooms);
        void clear();
        int roomidToIndex(QString roomid)
        {
                for (int i = 0; i < (int)roomids.size(); i++) {
                        if (roomids[i] == roomid)
                                return i;
                }

                return -1;
        }
        void joinPreview(QString roomid, QString parentSpace);
        void acceptInvite(QString roomid);
        void declineInvite(QString roomid);
        void leave(QString roomid);
        TimelineModel *currentRoom() const { return currentRoom_.get(); }
        RoomPreview currentRoomPreview() const
        {
                return currentRoomPreview_.value_or(RoomPreview{});
        }
        void setCurrentRoom(QString roomid);
        void resetCurrentRoom()
        {
                currentRoom_ = nullptr;
                currentRoomPreview_.reset();
                emit currentRoomChanged();
        }

private slots:
        void updateReadStatus(const std::map<QString, bool> roomReadStatus_);

signals:
        void totalUnreadMessageCountUpdated(int unreadMessages);
        void currentRoomChanged();
        void fetchedPreview(QString roomid, RoomInfo info);

private:
        void addRoom(const QString &room_id, bool suppressInsertNotification = false);
        void fetchPreview(QString roomid) const;

        TimelineViewManager *manager = nullptr;
        std::vector<QString> roomids;
        QHash<QString, RoomInfo> invites;
        QHash<QString, QSharedPointer<TimelineModel>> models;
        std::map<QString, bool> roomReadStatus;
        QHash<QString, std::optional<RoomInfo>> previewedRooms;

        QSharedPointer<TimelineModel> currentRoom_;
        std::optional<RoomPreview> currentRoomPreview_;

        friend class FilteredRoomlistModel;
};

class FilteredRoomlistModel : public QSortFilterProxyModel
{
        Q_OBJECT
        Q_PROPERTY(TimelineModel *currentRoom READ currentRoom NOTIFY currentRoomChanged RESET
                     resetCurrentRoom)
        Q_PROPERTY(RoomPreview currentRoomPreview READ currentRoomPreview NOTIFY currentRoomChanged
                     RESET resetCurrentRoom)
public:
        FilteredRoomlistModel(RoomlistModel *model, QObject *parent = nullptr);
        bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
        bool filterAcceptsRow(int sourceRow, const QModelIndex &) const override;

public slots:
        int roomidToIndex(QString roomid)
        {
                return mapFromSource(roomlistmodel->index(roomlistmodel->roomidToIndex(roomid)))
                  .row();
        }
        void joinPreview(QString roomid)
        {
                roomlistmodel->joinPreview(roomid, filterType == FilterBy::Space ? filterStr : "");
        }
        void acceptInvite(QString roomid) { roomlistmodel->acceptInvite(roomid); }
        void declineInvite(QString roomid) { roomlistmodel->declineInvite(roomid); }
        void leave(QString roomid) { roomlistmodel->leave(roomid); }
        void toggleTag(QString roomid, QString tag, bool on);

        TimelineModel *currentRoom() const { return roomlistmodel->currentRoom(); }
        RoomPreview currentRoomPreview() const { return roomlistmodel->currentRoomPreview(); }
        void setCurrentRoom(QString roomid) { roomlistmodel->setCurrentRoom(std::move(roomid)); }
        void resetCurrentRoom() { roomlistmodel->resetCurrentRoom(); }

        void nextRoom();
        void previousRoom();

        void updateFilterTag(QString tagId)
        {
                if (tagId.startsWith("tag:")) {
                        filterType = FilterBy::Tag;
                        filterStr  = tagId.mid(4);
                } else if (tagId.startsWith("space:")) {
                        filterType = FilterBy::Space;
                        filterStr  = tagId.mid(6);
                } else {
                        filterType = FilterBy::Nothing;
                        filterStr.clear();
                }

                invalidateFilter();
        }

        void updateHiddenTagsAndSpaces();

signals:
        void currentRoomChanged();

private:
        short int calculateImportance(const QModelIndex &idx) const;
        RoomlistModel *roomlistmodel;
        bool sortByImportance = true;

        enum class FilterBy
        {
                Tag,
                Space,
                Nothing,
        };
        QString filterStr   = "";
        FilterBy filterType = FilterBy::Nothing;
        QStringList hiddenTags, hiddenSpaces;
};