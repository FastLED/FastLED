# Json UI Components

Provides UI Components which emit Json events when they are added and destroyed.

A JsonUIManager will take care of updating internal state.

The JsonUiInternal will have the shared stated. The owning class is responsible
for determining how to parse the Json data and setting it's value.

To handle communication back and forth between the sketch and the platform
you will need to define the following:

```
void addJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) __attribute__((weak));
void removeJsonUiComponent(fl::WeakPtr<JsonUiInternal> component) __attribute__((weak));
```
