// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System.Collections.Generic;

namespace KeyboardManagerEditorUI.Helpers
{
    /// <summary>One row in the auto-switch dialog: a detected keyboard and its assigned profile.</summary>
    public sealed class KeyboardAssignmentRow
    {
        public string DisplayName { get; set; } = string.Empty;

        public string DevicePath { get; set; } = string.Empty;

        public IReadOnlyList<string> Profiles { get; set; } = new List<string>();

        public string SelectedProfile { get; set; } = string.Empty;
    }
}
