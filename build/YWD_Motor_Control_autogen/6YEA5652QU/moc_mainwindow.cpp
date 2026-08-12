/****************************************************************************
** Meta object code from reading C++ file 'mainwindow.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../include/mainwindow.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mainwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_MainWindow_t {
    QByteArrayData data[23];
    char stringdata0[262];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_MainWindow_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_MainWindow_t qt_meta_stringdata_MainWindow = {
    {
QT_MOC_LITERAL(0, 0, 10), // "MainWindow"
QT_MOC_LITERAL(1, 11, 14), // "onDeviceToggle"
QT_MOC_LITERAL(2, 26, 0), // ""
QT_MOC_LITERAL(3, 27, 11), // "onMitToggle"
QT_MOC_LITERAL(4, 39, 9), // "onMitTick"
QT_MOC_LITERAL(5, 49, 10), // "onPvToggle"
QT_MOC_LITERAL(6, 60, 8), // "onPvTick"
QT_MOC_LITERAL(7, 69, 10), // "onCvToggle"
QT_MOC_LITERAL(8, 80, 8), // "onCvTick"
QT_MOC_LITERAL(9, 89, 15), // "onSendSystemCmd"
QT_MOC_LITERAL(10, 105, 7), // "cmdCode"
QT_MOC_LITERAL(11, 113, 9), // "onRegRead"
QT_MOC_LITERAL(12, 123, 10), // "onRegWrite"
QT_MOC_LITERAL(13, 134, 13), // "onReadAllRegs"
QT_MOC_LITERAL(14, 148, 7), // "uint8_t"
QT_MOC_LITERAL(15, 156, 8), // "motor_id"
QT_MOC_LITERAL(16, 165, 15), // "onBatchReadNext"
QT_MOC_LITERAL(17, 181, 16), // "sendNextRegBlock"
QT_MOC_LITERAL(18, 198, 18), // "resendCurrentBlock"
QT_MOC_LITERAL(19, 217, 15), // "onFrameReceived"
QT_MOC_LITERAL(20, 233, 10), // "CanFdFrame"
QT_MOC_LITERAL(21, 244, 5), // "frame"
QT_MOC_LITERAL(22, 250, 11) // "onPollTimer"

    },
    "MainWindow\0onDeviceToggle\0\0onMitToggle\0"
    "onMitTick\0onPvToggle\0onPvTick\0onCvToggle\0"
    "onCvTick\0onSendSystemCmd\0cmdCode\0"
    "onRegRead\0onRegWrite\0onReadAllRegs\0"
    "uint8_t\0motor_id\0onBatchReadNext\0"
    "sendNextRegBlock\0resendCurrentBlock\0"
    "onFrameReceived\0CanFdFrame\0frame\0"
    "onPollTimer"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MainWindow[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      16,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   94,    2, 0x08 /* Private */,
       3,    0,   95,    2, 0x08 /* Private */,
       4,    0,   96,    2, 0x08 /* Private */,
       5,    0,   97,    2, 0x08 /* Private */,
       6,    0,   98,    2, 0x08 /* Private */,
       7,    0,   99,    2, 0x08 /* Private */,
       8,    0,  100,    2, 0x08 /* Private */,
       9,    1,  101,    2, 0x08 /* Private */,
      11,    0,  104,    2, 0x08 /* Private */,
      12,    0,  105,    2, 0x08 /* Private */,
      13,    1,  106,    2, 0x08 /* Private */,
      16,    0,  109,    2, 0x08 /* Private */,
      17,    0,  110,    2, 0x08 /* Private */,
      18,    0,  111,    2, 0x08 /* Private */,
      19,    1,  112,    2, 0x08 /* Private */,
      22,    0,  115,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   10,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 14,   15,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 20,   21,
    QMetaType::Void,

       0        // eod
};

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MainWindow *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onDeviceToggle(); break;
        case 1: _t->onMitToggle(); break;
        case 2: _t->onMitTick(); break;
        case 3: _t->onPvToggle(); break;
        case 4: _t->onPvTick(); break;
        case 5: _t->onCvToggle(); break;
        case 6: _t->onCvTick(); break;
        case 7: _t->onSendSystemCmd((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 8: _t->onRegRead(); break;
        case 9: _t->onRegWrite(); break;
        case 10: _t->onReadAllRegs((*reinterpret_cast< uint8_t(*)>(_a[1]))); break;
        case 11: _t->onBatchReadNext(); break;
        case 12: _t->sendNextRegBlock(); break;
        case 13: _t->resendCurrentBlock(); break;
        case 14: _t->onFrameReceived((*reinterpret_cast< const CanFdFrame(*)>(_a[1]))); break;
        case 15: _t->onPollTimer(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_MainWindow.data,
    qt_meta_data_MainWindow,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_MainWindow.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 16)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 16;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 16)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 16;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
