# pylint: disable=C0111,R0903

"""System-wide APM (Actions Per Minute) Tracker

Displays the APM (Actions Per Minute) measured by the apm_tracker service.

Parameters:
    * apm_tracker.interval: Time period to display (defaults to "hour", options: "minute", "hour", "day", "week")
    * apm_tracker.prefix: Prefix to display (defaults to "APM")
    * apm_tracker.refresh: Refresh interval in seconds (defaults to 1)

Requires:
    * apm_stats: Command-line utility for the apm_tracker service
"""

import subprocess
import re
import core.module
import core.widget
import core.input
import core.decorators
import time

class Module(core.module.Module):
    def __init__(self, config, theme):
        super().__init__(config, theme, core.widget.Widget(self.output))
        
        self._interval = self.parameter("interval", "hour")
        self._prefix = self.parameter("prefix", "APM")
        self._refresh = int(self.parameter("refresh", "1"))
        self._apm = "N/A"
        self._error = None
        self._last_update = 0
        
    @core.decorators.scrollable
    def output(self, _):
        # Include time period in the output
        if self._error:
            return "{}: {} ({})".format(self._prefix, self._apm, self._error)
        
        if self._interval == "minute":
            return "{} last minute: {}".format(self._prefix, self._apm)
        elif self._interval == "hour":
            return "{} last hour: {}".format(self._prefix, self._apm)
        elif self._interval == "day":
            return "{} last day: {}".format(self._prefix, self._apm)
        elif self._interval == "week":
            return "{} last week: {}".format(self._prefix, self._apm)
        else:
            return "{}: {}".format(self._prefix, self._apm)
        
    def update(self):
        # Only update if refresh interval has passed
        current_time = time.time()
        if current_time - self._last_update < self._refresh:
            return
        
        self._last_update = current_time
        self._error = None
        
        try:
            # Get the time period to search for
            if self._interval == "minute":
                search_pattern = r"Last 1 minute:\s+(\d+\.\d+) APM"
            elif self._interval == "hour":
                search_pattern = r"Last hour:\s+(\d+\.\d+) APM|Last 1 hour:\s+(\d+\.\d+) APM"
            elif self._interval == "day":
                search_pattern = r"Last 24 hours:\s+(\d+\.\d+) APM"
            elif self._interval == "week":
                search_pattern = r"Last 7 days:\s+(\d+\.\d+) APM"
            else:
                # Default to hour
                search_pattern = r"Last hour:\s+(\d+\.\d+) APM|Last 1 hour:\s+(\d+\.\d+) APM"
                
            # Run apm_stats command
            result = subprocess.run(["apm_stats"], capture_output=True, text=True)
            if result.returncode == 0:
                # Parse the output
                output = result.stdout.strip()
                match = re.search(search_pattern, output)
                if match:
                    # Handle the "hour" case which has alternates in the regex
                    if self._interval == "hour" and match.group(2):
                        self._apm = match.group(2)  # "Last 1 hour" format
                    elif match.group(1):
                        self._apm = match.group(1)  # First capture group for all other patterns
                    else:
                        self._apm = "0.00"
                        self._error = "No match"
                else:
                    self._apm = "0.00"
                    self._error = "No pattern match"
            else:
                self._apm = "Error"
                self._error = "Command failed"
        except Exception as e:
            self._apm = "Error"
            self._error = str(e)
            
    def state(self, _):
        if self._error:
            return "warning"
        return None

# vim: tabstop=4 expandtab shiftwidth=4 softtabstop=4