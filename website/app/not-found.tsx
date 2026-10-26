import Link from "next/link";
import { Shell } from "@/components/Shell";
import { dict } from "@/lib/i18n";

export default function NotFound() {
  const t = dict.en.notFound;
  const pl = dict.pl.notFound;
  return (
    <Shell lang="en">
      <div className="wrap page">
        <h1>404</h1>
        <p className="lead">{t.title}. {t.body}</p>
        <p className="muted">{pl.title}. {pl.body}</p>
        <p><Link href="/" className="btn btn-primary">{t.home}</Link></p>
      </div>
    </Shell>
  );
}
