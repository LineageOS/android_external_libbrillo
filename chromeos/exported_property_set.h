// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_EXPORTED_PROPERTY_SET_H_
#define LIBCHROMEOS_CHROMEOS_EXPORTED_PROPERTY_SET_H_

#include <map>
#include <string>
#include <vector>

#include <base/memory/weak_ptr.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>

namespace chromeos {

namespace dbus_utils {

// This class may be used to implement the org.freedesktop.DBus.Properties
// interface.  It sends the update signal on property updates:
//
//   org.freedesktop.DBus.Properties.PropertiesChanged (
//       STRING interface_name,
//       DICT<STRING,VARIANT> changed_properties,
//       ARRAY<STRING> invalidated_properties);
//
//
// and implements the required methods of the interface:
//
//   org.freedesktop.DBus.Properties.Get(in STRING interface_name,
//                                       in STRING property_name,
//                                       out VARIANT value);
//   org.freedesktop.DBus.Properties.Set(in STRING interface_name,
//                                       in STRING property_name,
//                                       in VARIANT value);
//   org.freedesktop.DBus.Properties.GetAll(in STRING interface_name,
//                                          out DICT<STRING,VARIANT> props);
//
//  This class is very similar to the PropertySet class in Chrome, except that
//  it allows objects to expose properties rather than to consume them.
//
//  Example usage:
//
//   class ExampleObjectExportingProperties {
//    public:
//     ExampleObjectExportingProperties(ExportedObject* exported_object)
//         : p_(exported_object) {
//       // Initialize properties appropriately.  Do this before
//       // claiming the Properties interface so that daemons watching
//       // this object don't see partial or inaccurate state.
//       p_.ClaimPropertiesInterface();
//     }
//
//    private:
//     struct Properties : public chromeos::dbus_utils::ExportedPropertySet {
//      public:
//       chromeos::dbus_utils::ExportedProperty<std::string> name_;
//       chromeos::dbus_utils::ExportedProperty<uint16> version_;
//       chromeos::dbus_utils::ExportedProperty<dbus::ObjectPath> parent_;
//       chromeos::dbus_utils::ExportedProperty<std::vector<std::string>>
//           children_;
//
//       Properties(dbus_utils::ExportedObject* exported_object)
//           : chromeos::dbus::ExportedPropertySet(exported_object) {
//         RegisterProperty(kExampleInterfaceName, "Name", &name_);
//         RegisterProperty(kExampleInterfaceName, "Version", &version_);
//         RegisterProperty(kExampleInterfaceName, "Parent", &parent_);
//         RegisterProperty(kExampleInterfaceName, "Children", &children_);
//       }
//       virtual ~Properties() {}
//     };
//
//     Properties p_;
//   };

class ExportedPropertyBase {
 public:
  ExportedPropertyBase() {}
  virtual ~ExportedPropertyBase() {}

  typedef base::Callback<void(const ExportedPropertyBase*)> OnUpdateCallback;

  // Called by ExportedPropertySet to register a callback.  This callback
  // triggers ExportedPropertySet to send a signal from the properties
  // interface of the exported object.
  virtual void SetUpdateCallback(const OnUpdateCallback& cb) = 0;

  // Appends a variant of the contained value to the writer.  This is
  // needed to write out properties to Get and GetAll methods implemented
  // by the ExportedPropertySet since it doesn't actually know the type
  // of each property.
  virtual void AppendValueToWriter(dbus::MessageWriter* writer) const = 0;
};

class ExportedPropertySet {
 public:
  typedef base::Callback<void(bool success)> OnInitFinish;
  typedef base::Callback<void(dbus::MessageWriter* writer)> PropertyWriter;

  ExportedPropertySet(dbus::Bus* bus, const dbus::ObjectPath& path);
  virtual ~ExportedPropertySet() = default;

  // Claims the method associated with the org.freedesktop.DBus.Properties
  // interface.  This needs to be done after all properties are initialized to
  // appropriate values.  This method will call |cb| when all methods
  // are exported to the DBus object.  |cb| will be called on the origin
  // thread.
  void Init(const OnInitFinish& cb);

  // Return a callback that knows how to write this property set's properties
  // to a message.  This writer retains a weak pointer to this, and must
  // only be invoked on the same thread as the rest of ExportedPropertySet.
  PropertyWriter GetPropertyWriter(const std::string& interface);

 protected:
  void RegisterProperty(const std::string& interface_name,
                        const std::string& property_name,
                        ExportedPropertyBase* exported_property);

 private:
  // Used to write the dictionary of string->variant to a message.
  // This dictionary represents the property name/value pairs for the
  // given interface.
  void WritePropertiesDictToMessage(const std::string& interface_name,
                                    dbus::MessageWriter* writer);
  void HandleGetAll(dbus::MethodCall* method_call,
                    dbus::ExportedObject::ResponseSender response_sender);
  void HandleGet(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender response_sender);
  // While Properties.Set has a handler to complete the interface,  we don't
  // support writable properties.  This is almost a feature, since bindings for
  // many languages don't support errors coming back from invalid writes.
  // Instead, use setters in exposed interfaces.
  void HandleSet(dbus::MethodCall* method_call,
                 dbus::ExportedObject::ResponseSender response_sender);
  void HandlePropertyUpdated(const std::string& interface,
                             const std::string& name,
                             const ExportedPropertyBase* property);

  dbus::Bus* bus_;
  dbus::ExportedObject* exported_object_;  // weak; owned by the Bus object.
  // This is a map from interface name -> property name -> pointer to property.
  std::map<std::string,
           std::map<std::string, ExportedPropertyBase*>> properties_;

  // D-Bus callbacks may last longer the property set exporting those methods.
  base::WeakPtrFactory<ExportedPropertySet> weak_ptr_factory_;

  friend class ExportedPropertySetTest;
  DISALLOW_COPY_AND_ASSIGN(ExportedPropertySet);
};

template <typename T>
class ExportedProperty : public ExportedPropertyBase {
 public:
  ExportedProperty();
  virtual ~ExportedProperty() override;

  // Retrieves the current value.
  const T& value() const;

  // Set the value exposed to remote applications.  This triggers notifications
  // of changes over the Properties interface.
  void SetValue(const T& new_value);

  // Called by ExportedPropertySet.  This update callback triggers
  // ExportedPropertySet to send a signal from the properties interface of the
  // exported object.
  virtual void SetUpdateCallback(const OnUpdateCallback& cb) override;

  // Implementation provided by specialization.
  virtual void AppendValueToWriter(dbus::MessageWriter* writer) const override;

 private:
  OnUpdateCallback on_update_;
  T value_{};

  DISALLOW_COPY_AND_ASSIGN(ExportedProperty);
};

extern template class ExportedProperty<bool>;
extern template class ExportedProperty<uint8>;
extern template class ExportedProperty<int16>;
extern template class ExportedProperty<uint16>;
extern template class ExportedProperty<int32>;
extern template class ExportedProperty<uint32>;
extern template class ExportedProperty<int64>;
extern template class ExportedProperty<uint64>;
extern template class ExportedProperty<double>;
extern template class ExportedProperty<std::string>;
extern template class ExportedProperty<dbus::ObjectPath>;
extern template class ExportedProperty<std::vector<std::string>>;
extern template class ExportedProperty<std::vector<dbus::ObjectPath>>;
extern template class ExportedProperty<std::vector<uint8>>;

}  // namespace dbus_utils

}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_EXPORTED_PROPERTY_SET_H_
