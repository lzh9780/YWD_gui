/****************************************************************************
** Meta object code from reading C++ file 'canfd_device.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../include/canfd_device.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'canfd_device.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_CanFdDevice_t {
    QByteArrayData data[10];
    char stringdata0[110];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_CanFdDevice_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_CanFdDevice_t qt_meta_stringdata_CanFdDevice = {
    {
QT_MOC_LITERAL(0, 0, 11), // "CanFdDevice"
QT_MOC_LITERAL(1, 12, 13), // "frameReceived"
QT_MOC_LITERAL(2, 26, 0), // ""
QT_MOC_LITERAL(3, 27, 10), // "CanFdFrame"
QT_MOC_LITERAL(4, 38, 5), // "frame"
QT_MOC_LITERAL(5, 44, 19), // "deviceStatusChanged"
QT_MOC_LITERAL(6, 64, 9), // "connected"
QT_MOC_LITERAL(7, 74, 12), // "startReceive"
QT_MOC_LITERAL(8, 87, 11), // "stopReceive"
QT_MOC_LITERAL(9, 99, 10) // "pollFrames"

    },
    "CanFdDevice\0frameReceived\0\0CanFdFrame\0"
    "frame\0deviceStatusChanged\0connected\0"
    "startReceive\0stopReceive\0pollFrames"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_CanFdDevice[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   39,    2, 0x06 /* Public */,
       5,    1,   42,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       7,    0,   45,    2, 0x0a /* Public */,
       8,    0,   46,    2, 0x0a /* Public */,
       9,    0,   47,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, QMetaType::Bool,    6,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void CanFdDevice::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<CanFdDevice *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->frameReceived((*reinterpret_cast< const CanFdFrame(*)>(_a[1]))); break;
        case 1: _t->deviceStatusChanged((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 2: _t->startReceive(); break;
        case 3: _t->stopReceive(); break;
        case 4: _t->pollFrames(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (CanFdDevice::*)(const CanFdFrame & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CanFdDevice::frameReceived)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (CanFdDevice::*)(bool );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CanFdDevice::deviceStatusChanged)) {
                *result = 1;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject CanFdDevice::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CanFdDevice.data,
    qt_meta_data_CanFdDevice,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *CanFdDevice::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *CanFdDevice::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CanFdDevice.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int CanFdDevice::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void CanFdDevice::frameReceived(const CanFdFrame & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void CanFdDevice::deviceStatusChanged(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
