import argparse
import os
import string


def copy_template_directory(src: str, dest: str, context: dict):
    for root, _, files in os.walk(src):
        for filename in files:
            src_path = os.path.join(root, filename)
            dest_path = os.path.join(
                root.replace(src, dest),
                render_to_string(filename, context),
            )
            content = render_template(src_path, context)
            save_to_disk(dest_path, content)


def render_to_string(template: str, context: dict):
    t = string.Template(template)
    return t.safe_substitute(context)


def render_template(path: str, context: dict):
    with open(path) as f:
        return render_to_string(f.read(), context)


def save_to_disk(path: str, content: str):
    os.makedirs(os.path.dirname(path), exist_ok=True)

    with open(path, "w") as f:
        f.write(content)


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Create a new Dripline project.",
    )

    parser.add_argument(
        "--proj_name",
        type=str,
        required=True,
        help="Name of the new project.",
    )

    parser.add_argument(
        "--location",
        type=str,
        required=False,
        default="./projects/",
        help="Location of the new project.",
    )

    return parser.parse_args()


def main():
    args = parse_arguments()

    ctx = {
        "location": args.location,
        "project_name": args.proj_name,
    }

    project_path = f"{ctx['location']}/{ctx['project_name']}"

    if os.path.exists(project_path):
        print(
            f"Err: A project named '{ctx['project_name']}' already exists in the '{ctx['location']}' directory."
        )
        v = input("Would you like to overwrite .vscode settings? (y/n): ")
        if v.lower() != "y":
            print("Aborting.")
        else:
            copy_template_directory(
                "./template/.vscode",
                f"{project_path}/.vscode",
                ctx,
            )
            print(".vscode settings overwritten.")
        return

    copy_template_directory("./template", project_path, ctx)


if __name__ == "__main__":
    main()
