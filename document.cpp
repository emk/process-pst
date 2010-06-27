#include <cstdint>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <pstsdk/pst.h>

#include "document.h"

using namespace std;
using boost::any;
using namespace boost::posix_time;
using namespace pstsdk;

namespace {
    // Rather than spend quite so much time checking for exceptions, let's
    // build a simple wrapper function which checks for us.
    template <typename R, typename T>
    bool has_prop(const pstsdk::message &m, R (T::*pmf)() const) {
        try {
            (m.*pmf)();
            return true;
        } catch (key_not_found<prop_id> &) {
            return false;
        }
    }
}

void document::initialize_from_message(const pstsdk::message &m) {
    property_bag props(m.get_property_bag());

    set_type(document::message);

    if (has_prop(m, &message::get_subject))
        (*this)[L"#Subject"] = wstring(m.get_subject());

    if (props.prop_exists(0x007d)) // PidTagTransportMessageHeaders
        (*this)[L"#Header"] = props.read_prop<wstring>(0x007d);

    if (props.prop_exists(0x0039)) // PidTagClientSubmitTime
        (*this)[L"#DateSent"] = from_time_t(props.read_time_t_prop(0x0039));

    if (props.prop_exists(0x0e06)) // PidTagMessageDeliveryTime
        (*this)[L"#DateReceived"] = from_time_t(props.read_time_t_prop(0x0e06));

    if (m.get_attachment_count() == 0) {
        (*this)[L"#HasAttachments"] = false;
        (*this)[L"#AttachmentCount"] = size_t(0);
    } else {
        (*this)[L"#HasAttachments"] = true;
        (*this)[L"#AttachmentCount"] = size_t(m.get_attachment_count());

        wstring names;
        message::attachment_iterator i(m.attachment_begin());
        for (; i != m.attachment_end(); ++i) {
            if (!names.empty())
                names += L";";
            names += i->get_filename();
        }
        (*this)[L"#AttachmentNames"] = names;
    }

    if (props.prop_exists(0x0e07)) // PidTagMessageFlags
        (*this)[L"#ReadFlag"] =
            (props.read_prop<int32_t>(0x0e07) & 0x1) ? true : false;

    if (props.prop_exists(0x0017)) // PidTagImportance
        (*this)[L"#ImportanceFlag"] =
            (props.read_prop<int32_t>(0x0017) > 1) ? true : false;

    if (props.prop_exists(0x001a)) // PidTagMessageClass
        (*this)[L"#MessageClass"] = props.read_prop<wstring>(0x001a);

    if (props.prop_exists(0x1090)) // PidTagFlagStatus
        (*this)[L"#FlagStatus"] = props.read_prop<int32_t>(0x1090);
}

document::document(const pstsdk::message &m) {
    initialize_from_message(m);
}

document::document(const pstsdk::attachment &a) {
    if (a.is_message()) {
        initialize_from_message(a.open_as_message());
    } else {
        set_type(document::file);
    
        wstring filename(a.get_filename());
        wstring extension;
        wstring::size_type dotpos(filename.rfind(L'.'));
        if (dotpos != wstring::npos)
            extension = filename.substr(dotpos + 1, wstring::npos);
 
        // Extract the native file.
        m_native = a.get_bytes();

        (*this)[L"#FileName"] = filename;
        (*this)[L"#FileExtension"] = extension;
        (*this)[L"#FileSize"] = uint64_t(native().size());

    }
}

any &document::operator[](const wstring &key) {
    return m_tags[key];
}
