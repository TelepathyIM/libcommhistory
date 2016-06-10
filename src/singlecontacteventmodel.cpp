/******************************************************************************
**
** This file is part of libcommhistory.
**
** Copyright (C) 2015 Jolla Ltd.
** Contact: Matt Vogt <matthew.vogt@jollamobile.com>
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the GNU Lesser General Public License version 2.1 as
** published by the Free Software Foundation.
**
** This library is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
** or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
** License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this library; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
**
******************************************************************************/

#include "singlecontacteventmodel.h"

#include "databaseio_p.h"
#include "commhistorydatabase.h"
#include "eventmodel_p.h"
#include "contactfetcher.h"

#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>

namespace CommHistory {

using namespace CommHistory;

class SingleContactEventModelPrivate : public EventModelPrivate
{
    Q_OBJECT

public:
    Q_DECLARE_PUBLIC(SingleContactEventModel)

    SingleContactEventModelPrivate(SingleContactEventModel *model);

    virtual bool acceptsEvent(const Event &event) const;

    virtual bool fillModel(int start, int end, QList<CommHistory::Event> events, bool resolved);

    ContactFetcher m_fetcher;
    int m_contactId;
    Recipient m_recipient;

public slots:
    void fetcherFinished();
};

SingleContactEventModelPrivate::SingleContactEventModelPrivate(SingleContactEventModel *model)
    : EventModelPrivate(model)
    , m_contactId(-1)
{
    connect(&m_fetcher, SIGNAL(finished()), SLOT(fetcherFinished()));
}

bool SingleContactEventModelPrivate::acceptsEvent(const Event &event) const
{
    return event.recipients().contactIds().contains(m_contactId);
}

bool SingleContactEventModelPrivate::fillModel(int start, int end, QList<CommHistory::Event> events, bool resolved)
{
    Q_UNUSED(end)

    // Filter out any events that did not resolve to our contact
    QList<CommHistory::Event>::iterator it = events.begin();
    while (it != events.end()) {
        const Event &event(*it);
        if (event.recipients().contactIds().contains(m_contactId)) {
            ++it;
        } else {
            it = events.erase(it);
        }
    }

    return EventModelPrivate::fillModel(start, start + events.size(), events, resolved);
}

void SingleContactEventModelPrivate::fetcherFinished()
{
    if (!m_recipient.isNull())
        m_contactId = m_recipient.contactId();

    // Our contact is now loaded in the cache
    RecipientList recipients = RecipientList::fromContact(m_contactId);
    if (!recipients.isEmpty()) {
        // Get the events that match these addresses
        QStringList clauses;
        QVariantList values;
        foreach (const Recipient &recipient, recipients) {
            if (recipient.localUid().isEmpty()) {
                clauses.append(QString("(remoteUid LIKE ? AND localUid LIKE '%1%%')").arg(RING_ACCOUNT));
                values.append(QString("%%%1%%").arg(minimizePhoneNumber(recipient.remoteUid())));
            } else {
                clauses.append("(remoteUid = ? AND localUid = ?)");
                values.append(recipient.remoteUid());
                values.append(recipient.localUid());
            }
        }

        QString where("WHERE ( ");
        where.append(clauses.join(" OR "));
        where.append(" ) ORDER BY Events.endTime DESC, Events.id DESC");

        QSqlQuery query = prepareQuery(DatabaseIOPrivate::eventQueryBase() + where);

        foreach (const QVariant &value, values)
            query.addBindValue(value);

        executeQuery(query);
    } else {
        modelUpdatedSlot(true);
    }
}

SingleContactEventModel::SingleContactEventModel(QObject *parent)
    : EventModel(*new SingleContactEventModelPrivate(this), parent)
{
    setResolveContacts(ResolveImmediately);
}

SingleContactEventModel::~SingleContactEventModel()
{
}

bool SingleContactEventModel::getEvents(int contactId)
{
    Q_D(SingleContactEventModel);

    beginResetModel();
    d->clearEvents();
    d->isReady = false;
    endResetModel();

    d->m_contactId = contactId;
    d->m_recipient = Recipient();
    d->m_fetcher.add(d->m_contactId);
    return true;
}

bool SingleContactEventModel::getEvents(const QContactId &contactId)
{
    Q_D(SingleContactEventModel);

    beginResetModel();
    d->clearEvents();
    d->isReady = false;
    endResetModel();

    d->m_contactId = SeasideCache::internalId(contactId);
    d->m_recipient = Recipient();
    d->m_fetcher.add(d->m_contactId);
    return true;
}

bool SingleContactEventModel::getEvents(const Recipient &recipient)
{
    Q_D(SingleContactEventModel);

    beginResetModel();
    d->clearEvents();
    d->isReady = false;
    endResetModel();

    d->m_contactId = -1;
    d->m_recipient = recipient;
    d->m_fetcher.add(d->m_recipient);
    return true;
}

} // namespace CommHistory

#include "singlecontacteventmodel.moc"

