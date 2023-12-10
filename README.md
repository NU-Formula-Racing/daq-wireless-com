# daq-firmware-base-24

This repository is the base repo, which daq projects should fork from as needed. The base is configured to work with ESP32-based projects, which may not be the case for your project.

**Checkout some learning resources [here!](./RESOURCES.md)**

## Important Notes
* For the **Logger, Wireless, and Dashboard Projects**, you will not be using the ESP32 enviornment configured here. Instead, you should go to ```platformio.ini``` and uncomment the teensy enviornment.
* In formula, we use a branching naming convention ```<initials>/<branch-name>```. So if I were to make a branch called ```analysis-backend```, it should be called ```ebs/analysis-backend```. Notice how we use kebab case, which is the standard in git related things.